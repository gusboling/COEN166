#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "environment.h"
#include "common.h"
#include "log.h"
#include "vegetation.h"

FILE *current_log_file;

#define LOG_FILE_SUFFIX ".log"

/* Create a new log file for the current environment.  */
int
open_log_file (const char *env_filename)
{
  size_t log_filename_len = strlen (RESULT_DIR_NAME)
                            + 1
                            + strlen (env_filename)
                            + strlen (LOG_FILE_SUFFIX)
                            + 1;

  char *log_filename = malloc (log_filename_len);

  sprintf (log_filename, RESULT_DIR_NAME "/%s" LOG_FILE_SUFFIX, env_filename);

  current_log_file = fopen (log_filename, "w");

  if (current_log_file == NULL)
    {
      fprintf (stderr, "Unable to create log file %s\n", log_filename);
      free (log_filename);
      return 0;
    }

  free (log_filename);
  return 1;
}

/* Print an ASCII representation of the current environment map, with the following
   representations:

   Agent:  A##  (## is agent's ID)
            D   (D is agent's direction)

   Predator:  P##  (## is predator species ID)

   Vegetation:  V##  (## is vegetation species ID)
                $$$  ($$$ is current bloom value, or 0 if not blooming)

*/
void
log_map (void)
{
  unsigned int row, col;

  /* Print top border.  */
  fputs ("   ", current_log_file);
  for (col = 0; col < env.size_x; col++)
    fprintf (current_log_file, " %03d", col);
  fputc ('\n', current_log_file);

  for (row = 0; row < env.size_y; row++)
    {
      fprintf (current_log_file, "%03d", row);

      for (col = 0; col < env.size_x; col++)
        {
          location_t *loc = &env.map[row][col];

          fputc (' ', current_log_file);

          switch (loc->occ_type)
            {
              case OCC_EMPTY:
                fputs ("   ", current_log_file);
                break;

              case OCC_AGENT:
                fprintf (current_log_file, "A%02d", loc->agent->id);
                break;

              case OCC_PRED:
                fprintf (current_log_file, "P%02d", loc->pred->pred_info->id);
                break;

              case OCC_VEG:
                fprintf (current_log_file, "V%02d", loc->veg.veg_info->id);
                break;
            }
        }

      fprintf (current_log_file, " %03d\n", row);

      fputs ("   ", current_log_file);
      for (col = 0; col < env.size_x; col++)
        {
          location_t *loc = &env.map[row][col];

          fputc (' ', current_log_file);

          switch (loc->occ_type)
            {
              case OCC_EMPTY:
              case OCC_PRED:
                fputs ("   ", current_log_file);
                break;

              case OCC_VEG:
                fprintf (current_log_file, "%3d", veg_bloom_value (&loc->veg));
                break;

              case OCC_AGENT:
                switch (loc->agent->dir)
                  {
                    case DIR_UP:
                      fputs (" ^ ", current_log_file);
                      break;

                    case DIR_DOWN:
                      fputs (" v ", current_log_file);
                      break;

                    case DIR_LEFT:
                      fputs (" < ", current_log_file);
                      break;

                    case DIR_RIGHT:
                      fputs (" > ", current_log_file);
                      break;
                  }
            }
        }
      fputc ('\n', current_log_file);
    }

  /* Print bottom border.  */
  fputs ("   ", current_log_file);
  for (col = 0; col < env.size_x; col++)
    fprintf (current_log_file, " %03d", col);
  fputc ('\n', current_log_file);
  fflush (current_log_file);
}

void
close_log_file (void)
{
  fclose (current_log_file);
}

