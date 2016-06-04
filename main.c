#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "environment.h"
#include "agent.h"
#include "log.h"

static int get_env_filenames (struct dirent ***env_files, int *num_env_files);
static int create_result_directory (void);
#ifdef __linux__
static int environment_file_filter (const struct dirent *env_file);
#else
static int environment_file_filter (struct dirent *env_file);
#endif
static void run_one_simulation (struct dirent *env_file);
static void run_one_turn (void);
static void summarize_results (void);

int main (int argc, char *argv[])
{
  int num_environment_files, next_env_file;
  struct dirent **environment_files;

#ifdef __linux__
  srandom ((int) time (NULL));
#else
  srandomdev ();
#endif

  if (! get_env_filenames (&environment_files, &num_environment_files))
    return 1;

  if (! create_result_directory ())
    return 1;

  if (! initialize_all_agents ())
    return 1;

  for (next_env_file = 0; next_env_file < num_environment_files; next_env_file++)
    {
      run_one_simulation (environment_files[next_env_file]);
    }

  release_all_agents ();

  return 0;
}

/* Read the names of all of the environment files.  */
static int
get_env_filenames (struct dirent ***env_files, int *num_env_files)
{
  *num_env_files = scandir (ENVIRONMENT_DIR_NAME, env_files,
                            environment_file_filter, alphasort);

  if (*num_env_files <= 0)
    {
      printf ("Unable to locate environment files (looked in '"
              ENVIRONMENT_DIR_NAME
              "').\n");
      return 0;
    }

  return 1;
}

int
create_result_directory (void)
{
  struct dirent **result_files;

  if (scandir (RESULT_DIR_NAME, &result_files, NULL, NULL) == -1)
    {
      if (mkdir (RESULT_DIR_NAME, S_IRWXU | S_IRWXG | S_IRWXO) == -1)
        {
          printf ("Unable to locate or create directory '" RESULT_DIR_NAME "'.\n");
          return 0;
        }
    }
  return 1;
}

/* Filter out all files beginning with a '.'  */
static int
#ifdef __linux__
environment_file_filter (const struct dirent *env_file)
#else
environment_file_filter (struct dirent *env_file)
#endif
{
  if (env_file->d_name[0] == '.')
    return 0;

  return 1;
}

/* Run a simulation, using the specified environment file.  */
static void
run_one_simulation (struct dirent *env_file)
{
  if (! open_log_file (env_file->d_name))
    return;

  LOG ((LOG_SCREEN | LOG_FILE), "*** Starting simulation %s ***", env_file->d_name);

  /* Parse the environment file.  */
  if (! initialize_from_env_file (env_file))
    return;

  /* Create all agents.  */
  if (! spawn_all_agents ())
    return;

  for (env.current_time = 0;
       env.current_time < env.simulation_length;
       env.current_time++)
    {
      LOG (LOG_FILE, " ");
      LOG ((LOG_SCREEN | LOG_FILE), "Starting Turn %u", env.current_time);
      LOG (LOG_FILE, " ");
      log_map ();
      LOG (LOG_FILE, " ");
      run_one_turn ();
      if (env.alive_agents == NULL)
        {
          LOG ((LOG_SCREEN | LOG_FILE), "  ** All agents have died -- aborting simulation");
          break;
        }
    }

  /* Clean up.  */
  summarize_results ();
  destroy_all_agents ();
  destroy_agent_list (&env.alive_agents);
  destroy_all_predators ();
  destroy_environment ();
  LOG ((LOG_SCREEN | LOG_FILE), "*** End of simulation %s ***\n", env_file->d_name);

  close_log_file ();
}

static void
run_one_turn (void)
{
  /* Ask each agent for its action.  */
  poll_all_agents ();

  /* Process all agent actions as a group.  */
  resolve_agent_actions ();

  /* Allow our predators to act.  */
  resolve_predator_actions ();

  /* If any of our predators reached their end of life, regenerate them.  */
  regenerate_predators ();
}

/* Print a summary in the log file at the end of a simulation.  */
static void
summarize_results (void)
{
  agent_list_t *agent_idx;

  LOG (LOG_FILE, "\nMap at end of simulation:");
  log_map ();

  LOG (LOG_FILE, "Summary of agent performance:");
  for (agent_idx = all_agents; agent_idx; agent_idx = agent_idx->next)
    {
      agent_t *agent = agent_idx->agent_data;

      if (agent->energy > 0)
        LOG (LOG_FILE, "%s: %u (energy of %u at end of simulation)", agent->directory_name,
             env.simulation_length + agent->energy, agent->energy);
      else
        LOG (LOG_FILE, "%s: %u (RIP)", agent->directory_name, agent->age_at_death);
    }
}

