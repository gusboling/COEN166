#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <sys/wait.h>

#include "environment.h"
#include "common.h"
#include "agent.h"
#include "log.h"

#ifdef __linux__
static int agent_directory_filter (const struct dirent *file_info);
#else
static int agent_directory_filter (struct dirent *file_info);
#endif
static void handle_agent_death (agent_t *agent);
static int create_new_pipe (const char *pipe_name);
static int send_initialization_command (agent_t *agent);
static int send_command (agent_t *agent, char *cmd, char *response,
                         size_t response_buf_size);
static int poll_one_agent (agent_t *agent);
static void generate_event_log_str (agent_t *agent, char *str);
static void parse_command (agent_t *agent, const char *action_str);
static void destroy_one_agent (agent_t *agent);
static void release_one_agent (agent_t *agent);

agent_list_t *all_agents = NULL;

/* Create FIFO pipes for input and output for each of the agents.  */
int
initialize_all_agents (void)
{
  struct dirent **agent_dirs;
  int num_agents;
  unsigned int agent_idx;
  unsigned int next_agent_id = 0;

  /* Find all agent directories.  */
  num_agents = scandir (AGENT_DIR_NAME, &agent_dirs, agent_directory_filter,
                        alphasort);

  if (num_agents == -1)
    {
      fprintf (stderr, "Unable to locate root agent directory.  Exiting...\n");
      return 0;
    }

  if (num_agents == 0)
    {
      fprintf (stderr, "Unable to find any agent directories.  Exiting...\n");
      return 0;
    }

  for (agent_idx = 0; agent_idx < num_agents; agent_idx++)
    {
      agent_t *new_agent;
      size_t pipe_name_len;

      new_agent = malloc (sizeof (agent_t));
      add_agent_to_list (new_agent, &all_agents);

      new_agent->id = next_agent_id++;

      /* Copy over directory name.  */
      new_agent->directory_name =
          malloc (strlen (agent_dirs[agent_idx]->d_name) + 1);
      strcpy (new_agent->directory_name, agent_dirs[agent_idx]->d_name);

      /* Configure agent input (percept) pipe */
      pipe_name_len = strlen (AGENT_DIR_NAME)
                      + 1
                      + strlen (new_agent->directory_name)
                      + 1
                      + strlen (PERCEPT_PIPE_NAME)
                      + 1;
      new_agent->percept_pipe_name = malloc (pipe_name_len);
      sprintf (new_agent->percept_pipe_name, AGENT_DIR_NAME "/%s/" PERCEPT_PIPE_NAME,
               new_agent->directory_name);
      if (! create_new_pipe (new_agent->percept_pipe_name))
        return 0;

      /* Configure agent output (action) pipe */
      pipe_name_len = strlen (AGENT_DIR_NAME)
                      + 1
                      + strlen (new_agent->directory_name)
                      + 1
                      + strlen (ACTION_PIPE_NAME)
                      + 1;
      new_agent->action_pipe_name = malloc (pipe_name_len);
      sprintf (new_agent->action_pipe_name, AGENT_DIR_NAME "/%s/" ACTION_PIPE_NAME,
               new_agent->directory_name);
      if (! create_new_pipe (new_agent->action_pipe_name))
        return 0;
    }

  return 1;
}

/* Create a FIFO pipe with the specified name, removing an existing file of the
   same name if one exists.  Returns 1 for success and 0 for failure.  */
static int
create_new_pipe (const char *pipe_name)
{
  struct stat stat_buf;

  /* Find and remove any existing file of the same name, if one exists.  */
  if ((stat (pipe_name, &stat_buf) == 0)
      && (unlink (pipe_name) == -1))
    {
      fprintf (stderr, "Unable to remove old pipe %s.\n", pipe_name);
      return 0;
    }

  if (mknod(pipe_name, S_IFIFO | 0666, 0) != 0)
    {
      fprintf (stderr, "Unable to create new pipe %s.\n", pipe_name);
      return 0;
    }

  return 1;
}

/* Specify which directories to include when searching for agents.
   Returns 1 to include a directory and 0 otherwise.  */
static int
#ifdef __linux__
agent_directory_filter (const struct dirent *file_info)
#else
agent_directory_filter (struct dirent *file_info)
#endif
{
  /* Ignore anything that's not a directory.  */
  if (file_info->d_type != DT_DIR)
    return 0;

  /* Ignore anything beginning with '.'  */
  if (file_info->d_name[0] == '.')
    return 0;

  return 1;
}

/* Drop an agent onto the map for the first time, at the specified
   location.  */
void
initialize_agent_location (agent_t *agent, unsigned int x, unsigned int y)
{
  agent->x_pos = x;
  agent->y_pos = y;

  assert (env.map[y][x].occ_type == OCC_EMPTY);

  env.map[y][x].occ_type = OCC_AGENT;
  env.map[y][x].agent = agent;
}

/* Change an agent's location on the map.  Note that the agent has information
   on where it is on the map, and the map has information on what is in each
   square, so both must be changed.  */
void
update_agent_location (agent_t *agent, unsigned int x, unsigned int y)
{
  location_t *old_loc = &env.map[agent->y_pos][agent->x_pos];
  location_t *new_loc = &env.map[y][x];

  assert (old_loc->occ_type == OCC_AGENT);
  assert (old_loc->agent == agent);

  old_loc->occ_type = OCC_EMPTY;
  new_loc->occ_type = OCC_AGENT;
  new_loc->agent = agent;

  agent->x_pos = x;
  agent->y_pos = y;
}

/* Handle a request to modify the energy level of an agent.  If the agent reaches
   zero, it is considered dead and is removed from the simulation.  */
void
modify_agent_energy (agent_t *agent, long energy_change, event_type_t event_type,
                     unsigned int id)
{
  long new_energy = (long) agent->energy + energy_change;
  agent_event_t *new_event;

  /* Agent died earlier in this turn.  We don't allow resurections, even in the
     same turn as a death.  */
  if (agent->energy <= 0)
    return;

  if (new_energy <= 0)
    {
      handle_agent_death (agent);
      return;
    }
  else
    agent->energy = new_energy;

  if (event_type == EVENT_UNLOGGED)
    return;

  /* Log a move event.  */
  if (event_type == EVENT_MOVED)
    {
      log_agent_move (agent);
      return;
    }

  /* Log any other type of event.  */
  new_event = malloc (sizeof (agent_event_t));
  new_event->next = agent->event_log;
  new_event->event_type = event_type;
  new_event->energy_change_info.id = id;
  new_event->energy_change_info.energy_delta = energy_change;
  agent->event_log = new_event;
}

/* Report an agent's death and remove it from the simulation.  It must also be
   removed from the alive_agents list in the environment so that we don't
   continue to request actions from it.  */
static void
handle_agent_death (agent_t *agent)
{
  LOG ((LOG_SCREEN | LOG_FILE), "  ** Agent %s has died!", agent->directory_name);
  agent->energy = 0;
  agent->age_at_death = env.current_time;

  /* Remove agent from the map.  */
  assert (env.map[agent->y_pos][agent->x_pos].occ_type == OCC_AGENT);
  assert (env.map[agent->y_pos][agent->x_pos].agent == agent);
  env.map[agent->y_pos][agent->x_pos].occ_type = OCC_EMPTY;

  remove_agent_from_list (agent, &env.alive_agents);

  /* Kill off the fork'd process that communicates with the scheme agent.  */
  destroy_one_agent (agent);
}

/* For each agent, initialize it, create the scm_agent process that will communicate
   between the agent and the environment, and send the initialization message.  */
int
spawn_all_agents (void)
{
  agent_list_t *agent_iter;

  LOG ((LOG_SCREEN | LOG_FILE), "Spawning agents");

  for (agent_iter = all_agents; agent_iter; agent_iter = agent_iter->next)
    {
      agent_t *agent = agent_iter->agent_data;

      if (! spawn_one_agent (agent))
        fprintf (stderr, "Unable to create agent %s -- skipping.\n",
                 agent->directory_name);
    }

  return 1;
}

/* Perform all of the initialization that must be done once per simulation
   for each agent.  This includes:
   * Fork off the child process that will communicate between the environment
     and the scheme-based agent.
   * Initialize the agent's values.
   * Open the FIFO pipes used for communication.
   * Send the initialization command to the agent.  */
int
spawn_one_agent (agent_t *agent)
{
  LOG (LOG_SCREEN, "  %s", agent->directory_name);
  LOG (LOG_FILE, "  %s (A%02u)", agent->directory_name, agent->id);

  agent->pid = fork();

  if (! agent->pid)
    {
      /* We are the child.  */
      char *argv[2] = { agent->directory_name, NULL };
      char *envp[1] = { NULL };

      execve ("scm_agent", argv, envp);

      fprintf (stderr, "Failed to create agent for %s\n",
               agent->directory_name);

      return 0;
    }

  agent->energy = env.default_energy;
  agent->action = ACTION_STAY;
  agent->event_log = NULL;

  /* Connect to the percept pipe (environment->agent).  */
  agent->percept_pipe_fd = open (agent->percept_pipe_name, O_WRONLY);
  if (agent->percept_pipe_fd == -1)
    {
      fprintf (stderr, "Unable to open %s for writing.\n",
               agent->percept_pipe_name);
      return 0;
    }

  /* Connect to the action pipe (agent->environment).  */
  agent->action_pipe_fd = open (agent->action_pipe_name, O_RDONLY);
  if (agent->action_pipe_fd == -1)
    {
      fprintf (stderr, "Unable to open %s for reading.\n",
               agent->action_pipe_name);
      return 0;
    }

  /* Tell the agent to initialize itself.  */
  if (! send_initialization_command (agent))
    return 0;

  /* Keep track of all agents that are still alive.  */
  add_agent_to_list (agent, &env.alive_agents);

  return 1;
}

/* Tell the specified agent to initialize itself.  */
static int
send_initialization_command (agent_t *agent)
{
  char response[MAX_CMD_SIZE];

  return send_command (agent, "(initialize-agent)", &response[0], MAX_CMD_SIZE);
}

/* Send a command to the specified agent, and expect a response.  If the response
   exceeds the size of the buffer, report an error.  */
static int
send_command (agent_t *agent, char *cmd, char *response,
              size_t response_buf_size)
{
  size_t cmd_len = strlen (cmd);
  ssize_t response_actual_len;

  if (write (agent->percept_pipe_fd, cmd, cmd_len) != cmd_len)
    {
      LOG ((LOG_SCREEN | LOG_FILE), "  ** Error sending message %s to agent %s!",
           cmd, agent->directory_name);
      LOG ((LOG_SCREEN | LOG_FILE), "  ** Killing agent %s!", agent->directory_name);
      handle_agent_death (agent);
      return 0;
    }

  LOG (LOG_FILE, "  -> %s", cmd);

  response_actual_len = read (agent->action_pipe_fd, response, response_buf_size);

  if (response_actual_len <= 0)
    {
      LOG ((LOG_SCREEN | LOG_FILE), "  ** Error %d reading response from agent %s!",
           (int) response_actual_len, agent->directory_name);
      LOG ((LOG_SCREEN | LOG_FILE), "  ** Killing agent %s!", agent->directory_name);
      handle_agent_death (agent);
      return 0;
    }

  if (response_actual_len >= response_buf_size)
    {
      LOG ((LOG_SCREEN | LOG_FILE), "  ** Error - response from agent %s exceeds maximum length!",
           agent->directory_name);
      LOG ((LOG_SCREEN | LOG_FILE), "  ** Killing agent %s!", agent->directory_name);
      handle_agent_death (agent);
      return 0;
    }

  response[response_actual_len] = '\0';
  LOG (LOG_FILE, "  <- %s", response);
  return 1;
}

/* Request a move from all agents.  */
void
poll_all_agents (void)
{
  agent_list_t *agent_iter;

  for (agent_iter = env.alive_agents; agent_iter; agent_iter = agent_iter->next)
    {
      agent_t *agent = agent_iter->agent_data;

      if (!agent->energy)
        continue;

      LOG (LOG_FILE, "  %s (A%02u)", agent->directory_name, agent->id);

      if (! poll_one_agent (agent))
        continue;
    }
}

/* Request a move from a single agent.  Returns 1 for success and 0 for
   failure.  */
static int
poll_one_agent (agent_t *agent)
{
  unsigned int distance;
  char buf[MAX_CMD_SIZE];
  char response[MAX_CMD_SIZE];
  char event_log_str[MAX_CMD_SIZE];
  char percept_str[MAX_CMD_SIZE];
  char *percept_str_ptr = &percept_str[0];

  /* Create the percept string.  */
  *percept_str_ptr++ = '\'';
  *percept_str_ptr++ = '(';
  for (distance = 1; distance <= env.visibility; distance++)
    gen_percept_at_distance (&percept_str_ptr, agent, distance);
  *percept_str_ptr++ = ')'; 
  *percept_str_ptr++ = '\0';

  /* Create the string representing the events that occurred during the last turn.  */
  generate_event_log_str (agent, &event_log_str[0]); 

  /* Send the message to the agent and read back the response.  */
  sprintf (buf, "(choose-action %u %s %s)", agent->energy, event_log_str, percept_str);
  if (! send_command (agent, buf, &response[0], MAX_CMD_SIZE))
    return 0;

  parse_command (agent, response);
  return 1;
}

/* Convert all events noted in the agent's event list into a string that will
   be passed to the agent as part of its percepts for the turn.  The event list
   will be empty after this function returns.  */
static void
generate_event_log_str (agent_t *agent, char *str)
{
  *str++ = '\'';
  *str++ = '(';

  while (agent->event_log)
    {
      agent_event_t *agent_event = agent->event_log;

      switch (agent_event->event_type)
        {
          case EVENT_MOVED:
            str += sprintf (str, "(moved %d)", agent_event->move_info.spaces);
            break;

          case EVENT_ATE:
            str += sprintf (str, "(ate %d %d)", agent_event->energy_change_info.id,
                            agent_event->energy_change_info.energy_delta);
            break;

          case EVENT_FOUGHT:
            str += sprintf (str, "(fought %d %d)", agent_event->energy_change_info.id,
                            agent_event->energy_change_info.energy_delta);
            break;

          case EVENT_ATTACKED:
            str += sprintf (str, "(attacked-by %d %d)", agent_event->energy_change_info.id,
                            agent_event->energy_change_info.energy_delta);
            break;

          case EVENT_UNLOGGED:
            break;
        }

      agent->event_log = agent->event_log->next;
      free (agent_event);
    }

  *str++ = ')';
  *str++ = '\0';
}

struct action_parse_elt {
  action_t act;
  const char *str;
} action_parse_table[] =
  { {ACTION_STAY,              "STAY"},
    {ACTION_TURN_RIGHT,        "TURN-RIGHT"},
    {ACTION_TURN_LEFT,         "TURN-LEFT"},
    {ACTION_TURN_AROUND,       "TURN-AROUND"},
    {ACTION_MOVE_PASSIVE_1,    "MOVE-PASSIVE-1"},
    {ACTION_MOVE_PASSIVE_2,    "MOVE-PASSIVE-2"},
    {ACTION_MOVE_PASSIVE_3,    "MOVE-PASSIVE-3"},
    {ACTION_MOVE_AGGRESSIVE_1, "MOVE-AGGRESSIVE-1"},
    {ACTION_MOVE_AGGRESSIVE_2, "MOVE-AGGRESSIVE-2"},
    {ACTION_MOVE_AGGRESSIVE_3, "MOVE-AGGRESSIVE-3"},
    {ACTION_EAT_PASSIVE,       "EAT-PASSIVE"},
    {ACTION_EAT_AGGRESSIVE,    "EAT-AGGRESSIVE"} };

/* Parse the action string provided by the agent, converting the value to
   our agent's internal value (action_t).  */
static void
parse_command (agent_t *agent, const char *action_str)
{
  unsigned int idx;

  for (idx = 0;
       idx < sizeof (action_parse_table) / sizeof (struct action_parse_elt);
       idx++)
    {
      if (! strcmp (action_str, action_parse_table[idx].str))
        {
          agent->action = action_parse_table[idx].act;
          add_agent_to_list (agent, &this_pass_actions);
          return;
        }
    }
    LOG (LOG_FILE, "  Agent returned invalid string '%s' - ignoring", action_str);
    agent->action = ACTION_STAY;
}

/* Is the agent currently moving?  If so, set aggressive_p to represent whether
   the move is aggressive.  */
int
agent_is_moving (agent_t *agent, int *aggressive_p)
{
  if (agent->action >= ACTION_MOVE_PASSIVE_1
      && agent->action <= ACTION_MOVE_PASSIVE_3)
    {
      *aggressive_p = 0;
      return 1;
    }

  if (agent->action >= ACTION_MOVE_AGGRESSIVE_1
      && agent->action <= ACTION_MOVE_AGGRESSIVE_3)
    {
      *aggressive_p = 1;
      return 1;
    }

  return 0;
}

/* Return a value indicating how many spaces the agent has requested to
   move.  If the agent has not requested a move, return 0.  */
unsigned int
number_agent_moves (agent_t *agent)
{
  if (agent->action >= ACTION_MOVE_PASSIVE_1
      && agent->action <= ACTION_MOVE_PASSIVE_3)
    return (agent->action - ACTION_MOVE_PASSIVE_1) + 1;

  if (agent->action >= ACTION_MOVE_AGGRESSIVE_1
      && agent->action <= ACTION_MOVE_AGGRESSIVE_3)
    return (agent->action - ACTION_MOVE_AGGRESSIVE_1) + 1;

  return 0;
}

/* Add an event log for an agent's move.  This is a little bit different from
   the other event logs because we only keep at most one move log per agent per
   turn, so we have to find an existing log if one exists and add to that one.  */
void
log_agent_move (agent_t *agent)
{
  agent_event_t *event_idx;
  agent_event_t *new_event;

  /* Search for an existing move event.  */
  for (event_idx = agent->event_log; event_idx; event_idx = event_idx->next)
    {
      if (event_idx->event_type == EVENT_MOVED)
        {
          /* Success!  Just increment the value there.  */
          event_idx->move_info.spaces++;
          return;
        }
    }

  /* No existing move event found, create a new event.  */
  new_event = malloc (sizeof (agent_event_t));
  new_event->next = agent->event_log;
  new_event->event_type = EVENT_MOVED;
  new_event->move_info.spaces = 1;
  agent->event_log = new_event;
}

/* Add an agent to the head of an agent list.  */
void
add_agent_to_list (agent_t *agent, agent_list_t **list)
{
  agent_list_t *new_node = malloc (sizeof (agent_list_t));
  new_node->agent_data = agent;
  new_node->next = *list;
  *list = new_node;
}

/* Remove the specified agent from an agent list.  Unfortunately, requires
   walking the entire list until we find the desired node.  Note that this
   will remove all instances of the agent from the list, even though we
   don't expect this to ever happen.  */
void
remove_agent_from_list (agent_t *agent, agent_list_t **list)
{
  while (*list)
    {
      if ((*list)->agent_data == agent)
        {
          agent_list_t *removed_node = *list;
          *list = (*list)->next;
          free (removed_node);
        }
      else
        list = &(*list)->next;
    }
}

/* Remove the first node from an agent list.  */
void
remove_agent_list_head (agent_list_t **list)
{
  agent_list_t *list_node;

  assert (*list != NULL);
  list_node = *list;
  *list = (*list)->next;
  free (list_node);
}

/* Remove all nodes from an agent list.  */
void
destroy_agent_list (agent_list_t **list)
{
  while (*list)
    remove_agent_list_head (list);
}

/* All agents are destroyed at the end of each simulation.  This means killing
   the scm_agent intermediary and cleaning up all simulation-specific data
   structures.  */
void
destroy_all_agents (void)
{
  agent_list_t *agent_iter;

  LOG ((LOG_SCREEN | LOG_FILE), "\nDestroying agents");

  for (agent_iter = env.alive_agents; agent_iter; agent_iter = agent_iter->next)
    {
      agent_t *agent = agent_iter->agent_data;

      LOG (LOG_SCREEN, "  %s", agent->directory_name);
      LOG (LOG_FILE, "  %s (A%02u)", agent->directory_name, agent->id);
      destroy_one_agent (agent_iter->agent_data);
    }
}

/* Clean up at the end of one simulation -- kill the intermediary
   scm_agent process and close out our FIFO pipes.  */
static void
destroy_one_agent (agent_t *agent)
{
  int stat_loc;

  /* Clear out the event log.  */
  while (agent->event_log)
    {
      agent_event_t *first_event_log = agent->event_log;
      agent->event_log = agent->event_log->next;
      free (first_event_log);
    }

  /* Kill the agent process.  */
  kill (agent->pid, SIGKILL);
  waitpid (agent->pid, &stat_loc, 0);

  /* Close our FIFOs.  */
  close (agent->percept_pipe_fd);
  close (agent->action_pipe_fd);
}

/* Handle agent cleanup after all simulations have been run.  */
void
release_all_agents (void)
{
  agent_list_t *agent_iter;

  agent_iter = all_agents;
  while (agent_iter != NULL)
    {
      agent_list_t *next_agent = agent_iter->next;
      release_one_agent (agent_iter->agent_data);
      free (agent_iter->agent_data);
      free (agent_iter);
      agent_iter = next_agent;
    }
}

/* Remove the files used for IPC after all simulations have been run.  */
static void
release_one_agent (agent_t *agent)
{
  unlink (agent->percept_pipe_name);
  unlink (agent->action_pipe_name);
}

