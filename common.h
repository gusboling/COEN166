#ifndef __COMMON_H__
#define __COMMON_H__

/* These are the names of the files that will be created in the agent's directory
   for IPC FIFOs (pipes).  */
#define PERCEPT_PIPE_NAME ".percepts"
#define ACTION_PIPE_NAME ".actions"

/* These are the "special" directory names where we expect to find specific
   information.  */
#define RESULT_DIR_NAME "Results"
#define ENVIRONMENT_DIR_NAME "Environments"
#define AGENT_DIR_NAME "Agents"

#define MAX_CMD_SIZE 1024

#endif /* ! defined (__COMMON_H__) */
