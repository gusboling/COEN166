#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libguile.h>

#include "common.h"

#define MAIN_SCM_FILENAME "main.scm"

/* This is the program that will act as the intermediary between a single scheme
   agent and the environment.  This program will link with the scheme interpreter
   library, accepting commands from the environment in one pipe and forwarding the
   output from the interpreter back to the environment in another pipe.  */
int main (int argc, char *argv[])
{
  size_t name_len;
  char *percept_pipe_name, *action_pipe_name;
  int percept_pipe_fd, action_pipe_fd;
  char cmd[MAX_CMD_SIZE];

  /* Connect to the percept (input) pipe.  */
  name_len = strlen (AGENT_DIR_NAME)
             + 1
             + strlen (argv[0])
             + 1
             + strlen (PERCEPT_PIPE_NAME)
             + 1;
  percept_pipe_name = malloc (name_len);
  sprintf (percept_pipe_name, AGENT_DIR_NAME "/%s/" PERCEPT_PIPE_NAME, argv[0]);
  percept_pipe_fd = open (percept_pipe_name, O_RDONLY);

  if (percept_pipe_fd == -1)
    {
      fprintf (stderr, "Unable to open %s for reading.\n", percept_pipe_name);
      return 1;
    }

  /* Connect to the action (output) pipe.  */
  name_len = strlen (AGENT_DIR_NAME)
             + 1
             + strlen (argv[0])
             + 1
             + strlen (ACTION_PIPE_NAME)
             + 1;
  action_pipe_name = malloc (name_len);
  sprintf (action_pipe_name, AGENT_DIR_NAME "/%s/" ACTION_PIPE_NAME, argv[0]);
  action_pipe_fd = open (action_pipe_name, O_WRONLY);

  if (action_pipe_fd == -1)
    {
      fprintf (stderr, "Unable to open %s for writing.\n", action_pipe_name);
      return 1;
    }

  /* Load the guile main.scm file.  */
  scm_init_guile ();
  snprintf (&cmd[0], MAX_CMD_SIZE, "(load \"%s/%s/" MAIN_SCM_FILENAME "\")",
            AGENT_DIR_NAME, argv[0]);
  scm_c_eval_string (cmd);

  /* Now, enter the main communication loop.  */
  while (1)
    {
      char input_buffer[MAX_CMD_SIZE];
      int bytes_read;
      SCM action_chosen;
      char *action_str;

      bytes_read = read (percept_pipe_fd, &input_buffer[0], MAX_CMD_SIZE);
      if (bytes_read == -1)
        {
          fprintf (stderr, "Error reading from pipe %s\n", action_pipe_name);
          return 1;
        }

      if (bytes_read >= MAX_CMD_SIZE)
        {
          fprintf (stderr, "Buffer not large enough to handle command!");
          return 1;
        }

      input_buffer[bytes_read] = '\0';

      action_chosen = scm_c_eval_string (input_buffer);
      if (scm_is_false (scm_string_p (action_chosen)))
        {
          fprintf (stderr, "Invalid response -- expected a string from the agent!\n");
          return 1;
        }

      action_str = scm_to_locale_string (action_chosen);

      write (action_pipe_fd, action_str, scm_c_string_length (action_chosen));
    }

  return 0;
}

