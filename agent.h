#ifndef __AGENT_H__
#define __AGENT_H__

#include <sys/types.h>

typedef enum {
  DIR_UP = 0,
  DIR_RIGHT,
  DIR_DOWN,
  DIR_LEFT
} direction_t;

typedef enum {
  ACTION_STAY,
  ACTION_TURN_RIGHT,
  ACTION_TURN_LEFT,
  ACTION_TURN_AROUND,
  ACTION_MOVE_PASSIVE_1,
  ACTION_MOVE_PASSIVE_2,
  ACTION_MOVE_PASSIVE_3,
  ACTION_MOVE_AGGRESSIVE_1,
  ACTION_MOVE_AGGRESSIVE_2,
  ACTION_MOVE_AGGRESSIVE_3,
  ACTION_EAT_PASSIVE,
  ACTION_EAT_AGGRESSIVE
} action_t;

typedef enum {
  EVENT_MOVED,
  EVENT_ATE,
  EVENT_FOUGHT,
  EVENT_ATTACKED,
  EVENT_UNLOGGED
} event_type_t;

typedef struct {
  unsigned int spaces;
} move_event_t;

typedef struct {
  unsigned int id;
  int energy_delta;
} energy_change_event_t;

/* Events that happen to agents, stored in lists and given as part of the
   agent's percepts each turn.  */
typedef struct agent_event {
  struct agent_event *next;
  event_type_t event_type;
  union {
    move_event_t move_info;
    energy_change_event_t energy_change_info;
  };
} agent_event_t;

/* Represents a single agent.  Note that this structure is persistent across all
   simulations.  */
typedef struct {

  /* i.e., life force.  If zero, the agent is currently dead.  */
  unsigned int energy;

  /* A unique identifier.  Mostly so other agents can recognize this one.  */
  unsigned int id;

  /* Where the agent is currently located in the map.  Note that this means that
     if I look at the environment's map, I better find this agent in that location
     (the two values are expected to be consistent).  */
  unsigned int x_pos, y_pos;  

  /* The absolute direction the agent is currently facing.  Note that the scheme
     interface will never know this -- all information is relative to its current
     orientation.  */
  direction_t dir;

  /* The action most recently selected by this agent.  */
  action_t action;

  /* A set of reportable events that happened to this agent during the last turn.
     These will be passed along to "choose-action" when the agent is invoked.  */
  agent_event_t *event_log;

  /* How many turns did this agent survive?  */
  unsigned int age_at_death;

  /* What is the directory name of this agent, relative to the top-level agents
     directory?  */
  char *directory_name;

  /* PID of the child that handles the communication with the scheme agent.  */
  pid_t pid;

  /* The pipe used to send percept information to the agent.  */
  char *percept_pipe_name;
  int percept_pipe_fd;

  /* The pipe used to receive the agent's action choice.  */
  char *action_pipe_name;
  int action_pipe_fd;
} agent_t;

typedef struct agent_list {
  struct agent_list *next;
  agent_t *agent_data;
} agent_list_t;

int initialize_all_agents (void);
void initialize_agent_location (agent_t *agent, unsigned int x, unsigned int y);
void update_agent_location (agent_t *agent, unsigned int x, unsigned int y);
void modify_agent_energy (agent_t *agent, long energy_change, event_type_t event_type,
                          unsigned int id);
int spawn_all_agents (void);
int spawn_one_agent (agent_t *agent);
void disable_agent (agent_t *agent);
void poll_all_agents (void);
int all_agents_dead (void);
int agent_is_moving (agent_t *agent, int *aggressive_p);
unsigned int number_agent_moves (agent_t *agent);
void log_agent_move (agent_t *agent);
void add_agent_to_list (agent_t *agent, agent_list_t **list);
void remove_agent_from_list (agent_t *agent, agent_list_t **list);
void remove_agent_list_head (agent_list_t **list);
void destroy_agent_list (agent_list_t **list);
void destroy_all_agents (void);
void release_all_agents (void);

extern agent_list_t *all_agents;

#endif /* ! defined (__AGENT_H__) */

