#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>

#include "common.h"
#include "log.h"
#include "environment.h"
#include "vegetation.h"

#define MAX_ATOM_SIZE 256
#define MAX_BLOOM_TABLE_SIZE 256

typedef enum {
  ATOM_UNRECOGNIZED,
  ATOM_DEFINE,
  ATOM_SIZE_X,
  ATOM_SIZE_Y,
  ATOM_DEFAULT_ENERGY,
  ATOM_SIMULATION_LENGTH,
  ATOM_VEGETATION,
  ATOM_NAME,
  ATOM_FREQUENCY,
  ATOM_INCUBATION,
  ATOM_BLOOM,
  ATOM_PREDATOR,
  ATOM_DAMAGE,
  ATOM_LIFE_SPAN,
  ATOM_NUM_INSTANCES
} atom_t;

struct atom_lookup_table_entry_t {
  const char *text;
  atom_t atom;
} atom_lookup_table[] =
  {{ "DEFINE",              ATOM_DEFINE             },
   { "SIZE-X:",             ATOM_SIZE_X             },
   { "SIZE-Y:",             ATOM_SIZE_Y             },
   { "DEFAULT-ENERGY:",     ATOM_DEFAULT_ENERGY     },
   { "SIMULATION-LENGTH:",  ATOM_SIMULATION_LENGTH  },
   { "VEGETATION",          ATOM_VEGETATION         },
   { "NAME:",               ATOM_NAME               },
   { "FREQUENCY:",          ATOM_FREQUENCY          },
   { "INCUBATION:",         ATOM_INCUBATION         },
   { "BLOOM:",              ATOM_BLOOM              },
   { "PREDATOR",            ATOM_PREDATOR           },
   { "DAMAGE:",             ATOM_DAMAGE             },
   { "LIFE-SPAN:",          ATOM_LIFE_SPAN          },
   { "NUM-INSTANCES:",      ATOM_NUM_INSTANCES      }};

struct environment_t env;
int atom_pushed_back_p;
atom_t deferred_atom;

/* Those agents whose moves we haven't yet handled.  */
agent_list_t *this_pass_actions = NULL;

static void create_map (void);
static void generate_agent_locations (void);
static void generate_predator_locations (void);
static void initialize_predator_location (pred_type_t *species, unsigned int x,
                                          unsigned int y);
static void generate_vegetation_locations (void);
static void gen_percept_sequence (char **str_ptr, direction_t agent_orientation,
                                  unsigned int num_locs, long x, long y,
                                  int delta_x, int delta_y);
static void gen_percept_loc (char **str_ptr, direction_t agent_orientation,
                             long x, long y);
static void gen_agent_percept (char **str_ptr, direction_t my_orientation,
                               agent_t *agent);
static void gen_pred_percept (char **str_ptr, pred_instance_t *pred);
static void gen_veg_percept (char **str_ptr, veg_instance_t *veg);
static void set_env_to_defaults (void);
static int parse_env_file (const char *filename);
static int parse_statement (FILE *infile);
static void skip_whitespace (FILE *infile);
static atom_t get_next_atom (FILE *infile);
static int parse_int (FILE *infile, int *result);
static int parse_uint (FILE *infile, unsigned int *result);
static int parse_vegetation (FILE *infile);
static int parse_uint_array (FILE *infile, unsigned int *table_size, int **table);
static void dump_bloom_table_to_logfile (bloom_schedule_t *sch);
static int parse_string (FILE *infile, char **result);
static int parse_float (FILE *infile, float *val);
static int parse_predator (FILE *infile);
static int attack_agent (pred_instance_t *pred, long x, long y);
static void predator_choose_move (pred_instance_t *pred);
static void predator_change_location (pred_instance_t *pred, long x, long y);
static unsigned int count_agents_in_direction (long start_x, long start_y, int x_delta,
                                               int y_delta, unsigned int num_rows);
static unsigned int count_agents_in_row (long start_x, long start_y, int x_delta,
                                         int y_delta, unsigned int num_locs,
                                         unsigned int agent_weight);
static void handle_pass (int pass_number, agent_list_t **this_pass_list,
                         agent_list_t **defer_list, agent_list_t **next_pass_list);
static void process_action_stay (agent_t *agent);
static void process_action_turn (agent_t *agent, direction_t direction);
static void process_action_move (agent_t *agent, int aggressive_p, unsigned int pass_number,
                                 agent_list_t **this_pass_list, agent_list_t **defer_list,
                                 agent_list_t **next_pass_list);
static int valid_location (long x, long y);
static void calculate_facing_location (agent_t *agent, long *x, long *y);
static void resolve_move_to_location (unsigned int pass_number, long x, long y,
                                      agent_list_t **this_pass_list, agent_list_t **defer_list,
                                      agent_list_t **next_pass_list);
static void calculate_movers_to_loc (long x, long y, agent_list_t **this_pass_list,
                                     agent_list_t **passive_movers,
                                     unsigned int *num_passive_movers,
                                     agent_list_t **aggressive_movers,
                                     unsigned int *num_aggressive_movers);
static void perform_move (agent_t *agent, long energy_used);
static void stop_agents (unsigned int pass_number, agent_list_t **agent_list);
static void resolve_move_fight (unsigned pass_number, agent_list_t **aggressive_movers,
                                agent_list_t **next_pass_list, int move_energy);
static void deduct_fight_cost (agent_list_t *aggressors);
static void process_action_eat (agent_t *agent, agent_list_t **this_pass_list, int aggressive_p);
static void calculate_eaters_of_loc (long x, long y, agent_list_t **this_pass_list,
                                     agent_list_t **passive_eaters,
                                     unsigned int *num_passive_eaters,
                                     agent_list_t **aggressive_eaters,
                                     unsigned int *num_aggressive_eaters);
static void resolve_food_fight (agent_list_t **aggressive_eaters, veg_instance_t *veg_instance);

/* Initialize the environment for a single simulation.  Parses the environment file
   and assigns locations to each of the agents, vegetation, and predators.  */
int
initialize_from_env_file (struct dirent *env_file)
{
  size_t filename_len;
  char *filename;
  int result;

  LOG ((LOG_SCREEN | LOG_FILE), "Initializing world");

  set_env_to_defaults ();

  filename_len = strlen (ENVIRONMENT_DIR_NAME)
                 + 1
                 + strlen (env_file->d_name)
                 + 1;
  filename = malloc (filename_len);

  sprintf (filename, ENVIRONMENT_DIR_NAME "/%s", env_file->d_name);

  result = parse_env_file (filename);
  free (filename);

  if (! result)
    return 0;

  create_map ();
  generate_agent_locations ();
  generate_predator_locations ();
  generate_vegetation_locations ();

  return result;
}

/* Generate our two-dimensional map of the environment, and mark all
   locations as empty.  */
static void
create_map (void)
{
  unsigned int row;

  env.map = malloc (sizeof (location_t *) * env.size_y);

  for (row = 0; row < env.size_y; row++)
    {
      unsigned int col;

      env.map[row] = malloc (sizeof (location_t) * env.size_x);

      for (col = 0; col < env.size_x; col++)
        env.map[row][col].occ_type = OCC_EMPTY;
    }
}

/* Assign initial locations to each of our agents.  */
static void
generate_agent_locations (void)
{
  agent_list_t *agent_iter;

  for (agent_iter = all_agents; agent_iter; agent_iter = agent_iter->next)
    {
      unsigned int x, y;

      /* If we expected a high ratio of agents:spaces, we might
         reconsider this approach.  */
      do {
        x = random () % env.size_x;
        y = random () % env.size_y;
      } while (env.map[y][x].occ_type != OCC_EMPTY);

      initialize_agent_location (agent_iter->agent_data, x, y);

      /* Randomly choose an orientation for the agent.  */
      agent_iter->agent_data->dir = (direction_t) (random () % 4);
    }
}

/* Assign a location for each of our predators.  */
static void
generate_predator_locations (void)
{
  pred_type_t *species;

  for (species = env.types_of_predators; species; species = species->next)
    {
      unsigned int pred_num;

      for (pred_num = 0; pred_num < species->num_instances; pred_num++)
        {
          unsigned int x, y;

          do {
            x = random () % env.size_x;
            y = random () % env.size_y;
          } while (env.map[y][x].occ_type != OCC_EMPTY);

          initialize_predator_location (species, x, y);
        }
    }
}

/* Create a new instance of a predator in the requested location.  Assign it a
   random age.  */
static void
initialize_predator_location (pred_type_t *species, unsigned int x, unsigned int y)
{
  pred_instance_t *new_pred = malloc (sizeof (pred_instance_t));
  new_pred->pred_info = species;
  new_pred->lifecycle_offset = random () % species->life_span;
  new_pred->x_pos = x;
  new_pred->y_pos = y;
  new_pred->next = env.all_predators;
  env.all_predators = new_pred;

  env.map[y][x].occ_type = OCC_PRED;
  env.map[y][x].pred = new_pred;
}

/* Search through our list of predator instances.  If any of them have reached
   their maximum age, regenerate them in a new, randomly-selected, location.  */
void
regenerate_predators (void)
{
  pred_instance_t *pred;

  for (pred = env.all_predators; pred; pred = pred->next)
    {
      /* We don't keep track of a predator's age, just it's age relative to
         time 0.  This way incrementing the time automatically increments
         each predator's age.  */
      if (((pred->lifecycle_offset + env.current_time)
           % pred->pred_info->life_span) == 0)
        {
          unsigned int x, y;

          /* Remove the predator from its current location.  */
          env.map[pred->y_pos][pred->x_pos].occ_type = OCC_EMPTY;

          /* Choose a new location.  */
          do {
            x = random () % env.size_x;
            y = random () % env.size_y;
          } while (env.map[y][x].occ_type != OCC_EMPTY);

          pred->x_pos = x;
          pred->y_pos = y;
          env.map[y][x].occ_type = OCC_PRED;
          env.map[y][x].pred = pred;
        }
    }
}

/* For each location in the map, randomly choose whether it will contain
   vegetation, using the probability provided in the environment file.  */
static void
generate_vegetation_locations (void)
{
  unsigned int x, y;

  for (y = 0; y < env.size_y; y++)
    for (x = 0; x < env.size_x; x++)
      {
        vegetation_list_t *veg_ptr = env.types_of_vegetation;

        while ((env.map[y][x].occ_type == OCC_EMPTY) && (veg_ptr != NULL))
          {
            double destiny = (float) (uint32_t) random () / (float) 0x7fffffff;

            if (destiny < veg_ptr->frequency)
              {
                env.map[y][x].occ_type = OCC_VEG;
                env.map[y][x].veg.veg_info = veg_ptr;

                /* Randomly choose an age for the plant.  */
                env.map[y][x].veg.lifecycle_offset = random ()
                    % veg_lifecycle_len (veg_ptr);
              }
            else
              veg_ptr = veg_ptr->next;
          }
      }
}

/* Generate a percept string for a specific distance from an agent.  Upon return,
   str_ptr will point at the NULL following the percept string.  */
void
gen_percept_at_distance (char **str_ptr, agent_t *agent, unsigned int distance)
{
  long start_x;
  long start_y;
  int delta_x;
  int delta_y;

  switch (agent->dir)
    {
      case DIR_UP:
        start_x = (long) agent->x_pos - distance;
        delta_x = 1;
        start_y = (long) agent->y_pos - distance;
        delta_y = 0;
        break;

      case DIR_DOWN:
        start_x = (long) agent->x_pos + distance;
        delta_x = -1;
        start_y = (long) agent->y_pos + distance;
        delta_y = 0;
        break;

      case DIR_LEFT:
        start_x = (long) agent->x_pos - distance;
        delta_x = 0;
        start_y = (long) agent->y_pos + distance;
        delta_y = -1;
        break;

      case DIR_RIGHT:
        start_x = (long) agent->x_pos + distance;
        delta_x = 0;
        start_y = (long) agent->y_pos - distance;
        delta_y = 1;
        break;

      default:
        assert (0);
        break;
    }

  gen_percept_sequence (str_ptr, agent->dir, (distance * 2) + 1,
                        start_x, start_y, delta_x, delta_y);
}

/* Generate a percept string for a given position (x/y), iterating num_locs
   and incrementing by (delta_x/delta_y) each time.  The specified agent
   orientation is required because we need to be able to calculate the
   relative orientation of other agents.  */
static void
gen_percept_sequence (char **str_ptr, direction_t agent_orientation,
                      unsigned int num_locs, long x, long y,
                      int delta_x, int delta_y)
{
  *(*str_ptr)++ = '(';

  gen_percept_loc (str_ptr, agent_orientation, x, y);

  while (--num_locs)
    {
      x += delta_x;
      y += delta_y;

      *(*str_ptr)++ = ' ';
      gen_percept_loc (str_ptr, agent_orientation, x, y);
    }

  *(*str_ptr)++ = ')';
}

/* Generate percept information for a single map location, given an agent's
   current orientation.  Writes the value into str_ptr and updates str_ptr to
   point to immediately following the data.  */
static void
gen_percept_loc (char **str_ptr, direction_t agent_orientation, long x, long y)
{
  if (! valid_location (x, y))
    *str_ptr += sprintf (*str_ptr, "barrier");
  else
    {
      location_t *loc = &env.map[y][x];

      switch (loc->occ_type)
        {
          case OCC_EMPTY:
            *str_ptr += sprintf (*str_ptr, "empty");
            break;

          case OCC_AGENT:
            gen_agent_percept (str_ptr, agent_orientation, loc->agent);
            break;

          case OCC_PRED:
            gen_pred_percept (str_ptr, loc->pred);
            break;

          case OCC_VEG:
            gen_veg_percept (str_ptr, &loc->veg);
            break;
        }
    }
}

/* Generate the percept atom for another agent, writing the value into str_ptr.  */
static void
gen_agent_percept (char **str_ptr, direction_t my_orientation, agent_t *agent)
{
  int chars_printed;
  unsigned int log2energy = 0;
  unsigned int agent_energy = agent->energy;
  const char *relative_orientation_strs[] = { "away", "right", "towards", "left" };
  int relative_orientation;
  const char *relative_orientation_str;

  relative_orientation = (direction_t) (((int) (agent->dir + 4) - my_orientation) % 4);
  relative_orientation_str = relative_orientation_strs[relative_orientation];

  while (agent_energy > 1)
    {
      log2energy++;
      agent_energy >>= 1;
    }

  chars_printed = sprintf (*str_ptr, "(agent %u %u %s)", agent->id, log2energy,
                           relative_orientation_str);
  *str_ptr += chars_printed;
}

/* Generate the percept atom for a predator, writing the value into str_ptr.  */
static void
gen_pred_percept (char **str_ptr, pred_instance_t *pred)
{
  int chars_printed;

  chars_printed = sprintf (*str_ptr, "(predator %u)", pred->pred_info->id);
  *str_ptr += chars_printed;
}

/* Generate the percept atom for vegetation, writing the value into str_ptr.  */
static void
gen_veg_percept (char **str_ptr, veg_instance_t *veg)
{
  int chars_printed;

  chars_printed = sprintf (*str_ptr, "(vegetation %u %d)", veg->veg_info->id,
                           veg_bloom_value (veg));
  *str_ptr += chars_printed;
}

/* Initialize environment default values.  */
static void
set_env_to_defaults (void)
{
  env.current_time = 0;
  env.simulation_length = 1000;
  env.size_x = 30;
  env.size_y = 30;
  env.default_energy = 500;
  env.visibility = AGENT_VISIBLE_DISTANCE;
  env.alive_agents = NULL;
  env.types_of_vegetation = NULL;
  env.types_of_predators = NULL;
  env.all_predators = NULL;
}

/* Read in an environment specification, and configure the current environment
   accordingly.  Each file is comprised of statements, separated by whitespace.  */
static int parse_env_file (const char *filename)
{
  FILE *env_file = fopen (filename, "r");
  int parse_result;

  if (env_file == NULL)
    {
      LOG (LOG_SCREEN, "** Error opening environment file %s\n", filename);
      return 0;
    }

  atom_pushed_back_p = 0;

  do {
    parse_result = parse_statement (env_file);
  } while (parse_result > 0);

  return !parse_result;
}

/* Read in a single statement from the specified file.
   Returns 1 if EOF was not reached, and at least one valid statement was processed.
   Returns 0 if EOF was reached.
   Returns -1 if a parse error occurred.  */
static int
parse_statement (FILE *infile)
{
  atom_t next_atom;

  skip_whitespace (infile);

  if (feof (infile))
    return 0;

  next_atom = get_next_atom (infile);

  if (next_atom != ATOM_DEFINE)
    {
      LOG (LOG_SCREEN, "** Error parsing environment file\n");
      return -1;
    }

  skip_whitespace (infile);

  if (feof (infile))
    return 0;

  next_atom = get_next_atom (infile);

  switch (next_atom)
    {
      case ATOM_SIZE_X:
        skip_whitespace (infile);
        if (! parse_uint (infile, &env.size_x))
          return -1;
        LOG (LOG_FILE, "  SIZE-X: %u", env.size_x);
        break;

      case ATOM_SIZE_Y:
        skip_whitespace (infile);
        if (! parse_uint (infile, &env.size_y))
          return -1;
        LOG (LOG_FILE, "  SIZE-Y: %u", env.size_y);
        break;

      case ATOM_DEFAULT_ENERGY:
        skip_whitespace (infile);
        if (! parse_uint (infile, &env.default_energy))
          return -1;
        LOG (LOG_FILE, "  DEFAULT-ENERGY: %u", env.default_energy);
        break;

      case ATOM_SIMULATION_LENGTH:
        skip_whitespace (infile);
        if (! parse_uint (infile, &env.simulation_length))
          return -1;
        LOG (LOG_FILE, "  SIMULATION-LENGTH: %u", env.simulation_length);
        break;

      case ATOM_VEGETATION:
        if (! parse_vegetation (infile))
          return -1;
        break;

      case ATOM_PREDATOR:
        if (! parse_predator (infile))
          return -1;
        break;

      default:
      case ATOM_UNRECOGNIZED:
        LOG (LOG_SCREEN, "** Error parsing environment file\n");
        return -1;
    }
  return 1;
}

static void
skip_whitespace (FILE *infile)
{
  char next_char;

  do {
    next_char = fgetc (infile);
  } while (isspace (next_char) && !feof (infile));

  if (feof (infile))
    return;

  ungetc (next_char, infile);
}

/* Read, and process, a single word from the environment file.  There are a very
   limited set of circumstances where we have to be able to look ahead, and never
   by more than a single word.  We use atom_pushed_back_p to reprsent that we
   have a token pre-read, and deferred_atom to represent that token.  */
static atom_t
get_next_atom (FILE *infile)
{
  char next_char;
  char atom_text[MAX_ATOM_SIZE];
  size_t atom_size = 0;
  size_t lookup_table_size = sizeof (atom_lookup_table)
                             / sizeof (struct atom_lookup_table_entry_t);
  size_t lookup_table_entry;

  if (atom_pushed_back_p)
    {
      atom_pushed_back_p = 0;
      return deferred_atom;
    }

  next_char = fgetc (infile);

  while (!isspace (next_char)
         && !feof (infile)
         && (atom_size < (MAX_ATOM_SIZE - 1)))
    {
      atom_text[atom_size++] = next_char;
      next_char = fgetc (infile);
    }

  atom_text[atom_size] = '\0';
  ungetc (next_char, infile);

  for (lookup_table_entry = 0;
       lookup_table_entry < lookup_table_size;
       lookup_table_entry++)
    {
      if (! strcmp (atom_lookup_table[lookup_table_entry].text,
                    atom_text))
        return atom_lookup_table[lookup_table_entry].atom;
    }

  return ATOM_UNRECOGNIZED;
}

/* Read in a signed integer value from the environment file.  */
static int
parse_int (FILE *infile, int *result)
{
  char next_char;
  int negated = 0;
  unsigned int uns_val;

  next_char = fgetc (infile);

  if (next_char == '-')
    negated = 1;
  else
    ungetc (next_char, infile);

  if (! parse_uint (infile, &uns_val))
    return 0;

  if (uns_val > INT_MAX)
    {
      LOG (LOG_SCREEN, "** Error parsing environment file");
      return 0;
    }

  if (negated)
    *result = - (int) uns_val;
  else
    *result = (int) uns_val;

  return 1;
}

/* Read in an unsigned integer value from the environment file.  */
static int
parse_uint (FILE *infile, unsigned int *result)
{
  char next_char;

  next_char = fgetc (infile);

  if (! isdigit (next_char))
    {
      LOG (LOG_SCREEN, "** Error parsing environment file\n");
      return 0;
    }

  *result = next_char - '0';
  next_char = fgetc (infile);

  while (isdigit (next_char) && !feof (infile))
    {
      *result = (*result * 10) + (next_char - '0');
      next_char = fgetc (infile);
    }

  if (!feof (infile) && !isspace (next_char))
    {
      LOG (LOG_SCREEN, "** Error parsing environment file\n");
      return 0;
    }

  ungetc (next_char, infile);
  return 1;
}

/* Read in a vegetation definition from the environment file.  */
static int
parse_vegetation (FILE *infile)
{
  static unsigned int unique_veg_id = 0;
  atom_t next_atom;
  vegetation_list_t *new_veg = malloc (sizeof (vegetation_list_t));

  LOG (LOG_FILE, "  VEGETATION");

  /* Defaults.  */
  new_veg->name = "ANONYMOUS";
  new_veg->id = unique_veg_id++;
  new_veg->frequency = 0.01;
  new_veg->bloom_schedule.incubation_time = 100;
  new_veg->bloom_schedule.bloom_table_size = 0;
  new_veg->bloom_schedule.bloom_table = NULL;

  while (1)
    {
      skip_whitespace (infile);

      if (feof (infile))
        goto success;

      next_atom = get_next_atom (infile);

      switch (next_atom)
        {
          case ATOM_DEFINE:
            atom_pushed_back_p = 1;
            deferred_atom = ATOM_DEFINE;
            goto success;

          case ATOM_NAME:
            skip_whitespace (infile);
            if (! parse_string (infile, &new_veg->name))
              goto failure;
            LOG (LOG_FILE, "    NAME: %s", new_veg->name);
            break;

          case ATOM_FREQUENCY:
            skip_whitespace (infile);
            if (! parse_float (infile, &new_veg->frequency))
              goto failure;
            LOG (LOG_FILE, "    FREQUENCY: %f", new_veg->frequency);
            break;

          case ATOM_INCUBATION:
            skip_whitespace (infile);
            if (! parse_uint (infile, &new_veg->bloom_schedule.incubation_time))
              goto failure;
            LOG (LOG_FILE, "    INCUBATION: %d", new_veg->bloom_schedule.incubation_time);
            break;

          case ATOM_BLOOM:
            skip_whitespace (infile);
            if (! parse_uint_array (infile, &new_veg->bloom_schedule.bloom_table_size,
                                    &new_veg->bloom_schedule.bloom_table))
              goto failure;
            dump_bloom_table_to_logfile (&new_veg->bloom_schedule);
            break;

          default:
          case ATOM_UNRECOGNIZED:
            LOG (LOG_SCREEN, "** Error parsing environment file\n");
            goto failure;
        }
    }

failure:
  free (new_veg);
  return 0;

success:
  new_veg->next = env.types_of_vegetation;
  env.types_of_vegetation = new_veg;
  return 1;
}

/* Read a sequence of integers from the environment file into an array.  */
static int
parse_uint_array (FILE *infile, unsigned int *table_size, int **table)
{
  *table_size = 0;
  int bloom_values[MAX_BLOOM_TABLE_SIZE];
  size_t ndx;
  char next_char;

  if (! parse_int (infile, &bloom_values[0]))
    return 0;
  (*table_size)++;

  skip_whitespace (infile);

  next_char = fgetc (infile);

  while (! feof (infile) && (isdigit (next_char) || (next_char == '-')))
    {
      ungetc (next_char, infile);
      if (! parse_int (infile, &bloom_values[*table_size]))
        return 0;
      (*table_size)++;
      skip_whitespace (infile);
      next_char = fgetc (infile);
    }

  ungetc (next_char, infile);
  *table = malloc (sizeof (int) * (*table_size));
  for (ndx = 0; ndx < *table_size; ndx++)
    (*table)[ndx] = bloom_values[ndx];

  return 1;
}

static void
dump_bloom_table_to_logfile (bloom_schedule_t *sch)
{
  int ndx;

  fprintf (current_log_file, "    BLOOM:");
  for (ndx = 0; ndx < sch->bloom_table_size; ndx++)
    fprintf (current_log_file, " %d", sch->bloom_table[ndx]);
  fputc ('\n', current_log_file);
}

/* Read a (whitespace-delimited) group of characters from the specified
   file.  */
static int
parse_string (FILE *infile, char **result)
{
  char next_char;
  char atom_text[MAX_ATOM_SIZE];
  size_t atom_size = 0;

  next_char = fgetc (infile);

  while (!isspace (next_char)
         && !feof (infile)
         && (atom_size < (MAX_ATOM_SIZE - 1)))
    {
      atom_text[atom_size++] = next_char;
      next_char = fgetc (infile);
    }

  atom_text[atom_size] = '\0';
  ungetc (next_char, infile);

  if (atom_size == 0)
    {
      LOG (LOG_SCREEN, "** Error parsing environment file\n");
      return 0;
    }

  *result = malloc (atom_size + 1);
  strcpy (*result, &atom_text[0]);
  return 1;
}

/* Read a simple floating-point value from the environment file.  */
static int
parse_float (FILE *infile, float *val)
{
  char next_char;
  float significance;

  if ((fgetc (infile) != '0') || (fgetc (infile) != '.'))
    {
      LOG (LOG_SCREEN, "** Error parsing environment file\n");
      return 0;
    }

  next_char = fgetc (infile);

  if (! isdigit (next_char))
    {
      LOG (LOG_SCREEN, "** Error parsing environment file\n");
      return 0;
    }

  *val = (next_char - '0') * 0.1f;
  significance = 0.01f;
  next_char = fgetc (infile);

  while (isdigit (next_char) && !feof (infile))
    {
      *val += ((next_char - '0') * significance);
      significance *= 0.1f;
      next_char = fgetc (infile);
    }

  if (!feof (infile) && !isspace (next_char))
    {
      LOG (LOG_SCREEN, "** Error parsing environment file\n");
      return 0;
    }

  ungetc (next_char, infile);
  return 1;
}

/* Read in a predator definition from the environment file.  */
static int
parse_predator (FILE *infile)
{
  static unsigned int unique_pred_id = 0;
  atom_t next_atom;
  pred_type_t *new_predator = malloc (sizeof (pred_type_t));

  LOG (LOG_FILE, "  PREDATOR");

  /* Defaults.  */
  new_predator->name = "ANONYMOUS";
  new_predator->id = unique_pred_id++;
  new_predator->damage = 10;
  new_predator->life_span = 10;
  new_predator->num_instances = 1;

  while (1)
    {
      skip_whitespace (infile);

      if (feof (infile))
        goto success;

      next_atom = get_next_atom (infile);

      switch (next_atom)
        {
          case ATOM_DEFINE:
            atom_pushed_back_p = 1;
            deferred_atom = ATOM_DEFINE;
            goto success;

          case ATOM_NAME:
            skip_whitespace (infile);
            if (! parse_string (infile, &new_predator->name))
              goto failure;
            LOG (LOG_FILE, "    NAME: %s", new_predator->name);
            break;

          case ATOM_DAMAGE:
            skip_whitespace (infile);
            if (! parse_uint (infile, &new_predator->damage))
              goto failure;
            LOG (LOG_FILE, "    DAMAGE: %u", new_predator->damage);
            break;

          case ATOM_LIFE_SPAN:
            skip_whitespace (infile);
            if (! parse_uint (infile, &new_predator->life_span))
              goto failure;
            LOG (LOG_FILE, "    LIFE-SPAN: %u", new_predator->life_span);
            break;

          case ATOM_NUM_INSTANCES:
            skip_whitespace (infile);
            if (! parse_uint (infile, &new_predator->num_instances))
              goto failure;
            LOG (LOG_FILE, "    NUM-INSTANCES: %u", new_predator->num_instances);
            break;

          default:
          case ATOM_UNRECOGNIZED:
            LOG (LOG_SCREEN, "** Error parsing environment file\n");
            goto failure;
        }
    }

failure:
  free (new_predator);
  return 0;

success:
  new_predator->next = env.types_of_predators;
  env.types_of_predators = new_predator;
  return 1;
}

/* Handle all of our agents' move selections.  We do this in multiple passes.  The first
   pass addresses the first one square of any moves, and all other actions.  Each
   additional pass handles the next square of all moves.  (e.g., a move of 2 locations
   is handled in the first and second pass).
   Note that this isn't quite sufficient, however, and that we have a concept of
   "deferred actions", which are those handed after all others in a pass.  Any time
   we have an agent who is moving into a square occupied by another agent that is
   also moving, we defer its action until after the other agent has a chance to move.
   If the other agent is also deferred, it's possible to have a circular loop in
   which case nobody will move, since deferred actions cannot be re-deferred.  */
void
resolve_agent_actions (void)
{
  /* Moves into spaces that are occupied, but may become vacant.  Defer
     these until after all other moves have occurred.  */
  agent_list_t *deferred_actions = NULL;

  /* Moves that are complete for this pass, but still require handling in the
     next pass.  */
  agent_list_t *next_pass_actions = NULL;

  unsigned pass_number = 1;

  while (this_pass_actions)
    {
      /* First half of the pass -- deferring moves as needed, and placing actions
         into "next_pass_actions" if we have completed all the required work for
         this pass, but there are still moves remaining for additional passes.  */
      handle_pass (pass_number, &this_pass_actions, &deferred_actions, &next_pass_actions);
      assert (this_pass_actions == NULL);

      /* Second half of the pass -- no more deferrals are allowed.  */
      handle_pass (pass_number, &deferred_actions, NULL, &next_pass_actions);
      assert (deferred_actions == NULL);

      /* Get ready for the next pass.  */
      this_pass_actions = next_pass_actions;
      next_pass_actions = NULL;
      pass_number++;
    }
}

/* Choose an action for each predator.  Predators will always attack all agents who
   are in adjacent squares.  If no agents are in adjacent squares, the predator will
   move in a direction it thinks most promising (based on the number of agents in
   each direction, weighted by proximity).  */
void
resolve_predator_actions (void)
{
  pred_instance_t *pred;
  int attacked_agent = 0;

  for (pred = env.all_predators; pred; pred = pred->next)
    {
      long x = pred->x_pos;
      long y = pred->y_pos;

      if (attack_agent (pred, x + 1, y))
        attacked_agent = 1;

      if (attack_agent (pred, x - 1, y))
        attacked_agent = 1;

      if (attack_agent (pred, x, y + 1))
        attacked_agent = 1;

      if (attack_agent (pred, x, y - 1))
        attacked_agent = 1;

      if (attacked_agent)
        continue;

      predator_choose_move (pred);
    }
}

/* Make a predator attack an agent in an adjacent square.  x/y represents the
   coordinates of the location the predator would like to attack.  */
static int
attack_agent (pred_instance_t *pred, long x, long y)
{
  location_t *loc;
  pred_type_t *species = pred->pred_info;

  if (! valid_location (x, y))
    return 0;

  loc = &env.map[y][x];

  if (loc->occ_type != OCC_AGENT)
    return 0;

  modify_agent_energy (loc->agent, - (long) (species->damage), EVENT_ATTACKED, species->id);
  return 1;
}

/* Choose the most promising move for a predator, and perform that move.  */
static void
predator_choose_move (pred_instance_t *pred)
{
  struct direction_offsets {
    direction_t dir;
    int x_delta;
    int y_delta;
  } choices[] =
    {{ DIR_UP,    0, -1 },
     { DIR_RIGHT, 1,  0 },
     { DIR_DOWN,  0,  1 },
     { DIR_LEFT, -1,  0 }};
  unsigned int iter;
  unsigned int best_choice = 0;
  unsigned int best_weight = 0;
  long new_x, new_y;

  for (iter = 0; iter < (sizeof (choices) / sizeof (struct direction_offsets)); iter++)
    {
      long x = pred->x_pos + choices[iter].x_delta;
      long y = pred->y_pos + choices[iter].y_delta;
      unsigned int weight;
      long row_x_delta, row_y_delta;

      if (! valid_location (x, y))
        continue;

      /* Black magic alert -- these formulas allow us to calculate the offsets of each
         row based on the offsets of the direction we are looking at.  */
      if (choices[iter].x_delta == 0)
        {
          row_x_delta = choices[iter].y_delta;
          row_y_delta = choices[iter].y_delta;
        }
      else
        {
          row_x_delta = choices[iter].x_delta;
          row_y_delta = -choices[iter].x_delta;
        }

      /* Note that this doesn't really "count" the agents, but creates a summation of
         the weighted importance of all agents in the given direction (with closer
         agents being weighted higher).  */
      weight = count_agents_in_direction (pred->x_pos, pred->y_pos, row_x_delta,
                                          row_y_delta, PREDATOR_VISIBLE_DISTANCE);

      if (weight > best_weight)
        {
          best_choice = iter;
          best_weight = weight;
        }
    }

  /* If no agents were seen (or no moves are possible), randomly choose a move.
     If no valid move is chosen after 10 attempts, give up and stay put.  */
  if (best_weight == 0)
    {
      unsigned int idx;

      for (idx = 0; idx < 10; idx++)
        {
          unsigned int move = random () % 4;
          new_x = pred->x_pos + choices[move].x_delta;
          new_y = pred->y_pos + choices[move].y_delta;

          if ((! valid_location (new_x, new_y))
              || (env.map[new_y][new_x].occ_type != OCC_EMPTY))
            continue;
          else
            break;
        }
      /* No valid location found -- stay put.  */
      return;
    }
  else
    {
      new_x = pred->x_pos + choices[best_choice].x_delta;
      new_y = pred->y_pos + choices[best_choice].y_delta;
    }

  predator_change_location (pred, new_x, new_y);
}

/* Move a predator to a new location.  */
static void
predator_change_location (pred_instance_t *pred, long x, long y)
{
  if (! valid_location (x, y))
    return;

  if (env.map[y][x].occ_type != OCC_EMPTY)
    return;

  assert (env.map[pred->y_pos][pred->x_pos].occ_type == OCC_PRED);
  assert (env.map[pred->y_pos][pred->x_pos].pred == pred);

  env.map[pred->y_pos][pred->x_pos].occ_type = OCC_EMPTY;
  env.map[y][x].occ_type = OCC_PRED;
  env.map[y][x].pred = pred;
  pred->x_pos = x;
  pred->y_pos = y;
}

/* Find the weighted sum of all agents in a given direction.  start_x/start_y
   tells us from whence we are looking.  x_delta/y_delta tells us in what
   direction we should move from our starting position to find the next row
   to search.  num_rows tells us how far out from the predator to look.  */
static unsigned int
count_agents_in_direction (long start_x, long start_y, int x_delta, int y_delta,
                           unsigned int num_rows)
{
  unsigned int row_num;
  unsigned int agents_found = 0;

  for (row_num = 0; row_num < num_rows; row_num++)
    {
      unsigned int agent_weight = (num_rows - row_num) * (num_rows - row_num);

      start_x += x_delta;
      start_y += y_delta;

      /* Black magic alert -- easy to calculate, but not necessarily intuitive
         means to convert our start location deltas into row deltas.  */
      if (x_delta == y_delta)
        agents_found += count_agents_in_row (start_x, start_y, -x_delta, 0,
                                             (row_num * 2) + 3, agent_weight);
      else
        agents_found += count_agents_in_row (start_x, start_y, 0, -y_delta,
                                             (row_num * 2) + 3, agent_weight);
    }

  return agents_found;
}

/* Generate the weighted sum of all agents in a given row, where the weight of
   each agent is given by "agent_weight".  To search, we start at 
   start_x/start_y and then move num_locs spaces, incrementing the position
   by x_delta/y_delta each time.  */
static unsigned int
count_agents_in_row (long start_x, long start_y, int x_delta, int y_delta,
                     unsigned int num_locs, unsigned int agent_weight)
{
  unsigned int agents_found = 0;

  while (num_locs--)
    {
      if (valid_location (start_x, start_y)
          && env.map[start_y][start_x].occ_type == OCC_AGENT)
        agents_found += agent_weight;

      start_x += x_delta;
      start_y += y_delta;
    }

  return agents_found;
}

/* Process the actions for all agents for a given pass.  All actions in this_pass_list
   should be handled, either by:
   * Removing if the action is completely handled
   * Moving the action to the defer_list if the action is a move to an occupied space
     containing an agent who is also moving
   * Moving the action to the next_pass_list if the action has been handled for this
     pass, but still requires handling in future passes
   If the "defer_list" is NULL, don't allow deferrals.  */
static void
handle_pass (int pass_number, agent_list_t **this_pass_list, agent_list_t **defer_list,
             agent_list_t **next_pass_list)
{
  while (*this_pass_list)
    {
      agent_t *agent = (*this_pass_list)->agent_data;

      switch (agent->action)
        {
          case ACTION_STAY:
            process_action_stay (agent);
            remove_agent_list_head (this_pass_list);
            break;

          case ACTION_TURN_LEFT:
            process_action_turn (agent, DIR_LEFT);
            remove_agent_list_head (this_pass_list);
            break;

          case ACTION_TURN_RIGHT:
            process_action_turn (agent, DIR_RIGHT);
            remove_agent_list_head (this_pass_list);
            break;

          case ACTION_TURN_AROUND:
            process_action_turn (agent, DIR_DOWN);
            remove_agent_list_head (this_pass_list);
            break;

          case ACTION_MOVE_PASSIVE_1:
          case ACTION_MOVE_PASSIVE_2:
          case ACTION_MOVE_PASSIVE_3:
            process_action_move (agent, 0, pass_number, this_pass_list, defer_list,
                                 next_pass_list);
            break;

          case ACTION_MOVE_AGGRESSIVE_1:
          case ACTION_MOVE_AGGRESSIVE_2:
          case ACTION_MOVE_AGGRESSIVE_3:
            process_action_move (agent, 1, pass_number, this_pass_list, defer_list,
                                 next_pass_list);
            break;

          case ACTION_EAT_PASSIVE:
            process_action_eat (agent, this_pass_list, 0);
            break;

          case ACTION_EAT_AGGRESSIVE:
            process_action_eat (agent, this_pass_list, 1);
            break;
        }
    }
}

/* Handle an agent who doesn't want to move.  */
static void
process_action_stay (agent_t *agent)
{
  modify_agent_energy (agent, -DEFAULT_STAY_COST, EVENT_UNLOGGED, 0);
}

/* Handle an agent's TURN action.  */
static void
process_action_turn (agent_t *agent, direction_t direction)
{
  agent->dir = (direction_t) ((agent->dir + (int) direction) % 4);
  modify_agent_energy (agent, -DEFAULT_TURN_COST, EVENT_UNLOGGED, 0);
}

/* Handle an agent's MOVE action.  */
static void
process_action_move (agent_t *agent, int aggressive_p, unsigned int pass_number,
                     agent_list_t **this_pass_list, agent_list_t **defer_list,
                     agent_list_t **next_pass_list)
{
  long facing_x, facing_y;
  location_t *loc;

  calculate_facing_location (agent, &facing_x, &facing_y);

  /* If the user has requested an invalid move, pretend it was a STAY.  */
  if (! valid_location (facing_x, facing_y))
    {
      /* An agent who requests a move of two spaces but is stopped after 1 should
         only be charged for a single space move, and not the extra cost of a
         STAY.  An agent who requests a single space move to an illegal location,
         however, should be charged for STAY, otherwise an illegal move would
         be preferrable to a STAY action.  */
      if (pass_number == 1)
        process_action_stay (agent);
      remove_agent_list_head (this_pass_list);
      return;
    }

  loc = &env.map[facing_y][facing_x];

  switch (loc->occ_type)
    {
      case OCC_EMPTY:
        resolve_move_to_location (pass_number, facing_x, facing_y, this_pass_list,
                                  defer_list, next_pass_list);
        break;

      case OCC_AGENT:
        {
          agent_t *other_agent = loc->agent;
          int aggressive_p;

          /* Punt to the defer list, if the other agent is moving.  */
          if ((defer_list)
              && agent_is_moving (other_agent, &aggressive_p))
            {
              add_agent_to_list (agent, defer_list);
            }
          else
            {
              if (pass_number == 1)
                process_action_stay (agent);
            }
          remove_agent_list_head (this_pass_list);
          break;
        }

      case OCC_PRED:
      case OCC_VEG:
        if (pass_number == 1)
          process_action_stay (agent);
        remove_agent_list_head (this_pass_list);
        return;
    }
}

/* Are the specified coordinates within the environment boundaries?  */
static int
valid_location (long x, long y)
{
  return (x >= 0)
         && (x < env.size_x)
         && (y >= 0)
         && (y < env.size_y);
}

/* Determine the location immediately in front of an agent, based on its
   current orientation.  */
static void
calculate_facing_location (agent_t *agent, long *x, long *y)
{
  switch (agent->dir)
    {
      case DIR_UP:
        *x = (long) agent->x_pos;
        *y = (long) agent->y_pos - 1;
        break;

      case DIR_RIGHT:
        *x = (long) agent->x_pos + 1;
        *y = (long) agent->y_pos;
        break;

      case DIR_DOWN:
        *x = (long) agent->x_pos;
        *y = (long) agent->y_pos + 1;
        break;

      case DIR_LEFT:
        *x = (long) agent->x_pos - 1;
        *y = (long) agent->y_pos;
        break;
    }
}

/* Resolve all requests to move to a given location.  */
static void
resolve_move_to_location (unsigned int pass_number, long x, long y,
                          agent_list_t **this_pass_list, agent_list_t **defer_list,
                          agent_list_t **next_pass_list)
{
  agent_list_t *passive_movers = NULL;
  unsigned int num_passive_movers = 0;
  agent_list_t *aggressive_movers = NULL;
  unsigned int num_aggressive_movers = 0;

  /* Find all movers that are targeting the specified destination, and add them
     into "passive_movers" and "aggressive_movers" lists.  */
  calculate_movers_to_loc (x, y, this_pass_list,
                           &passive_movers, &num_passive_movers,
                           &aggressive_movers, &num_aggressive_movers);

  /* Also search the deferrals, and add them into passive/aggressive_movers as
     needed.  Note that this may be possible if the agent who caused a
     deferral has moved since the deferral occurred.  */
  if (defer_list)
    calculate_movers_to_loc (x, y, defer_list,
                             &passive_movers, &num_passive_movers,
                             &aggressive_movers, &num_aggressive_movers);

  /* This function is invoked when an agent tries to move into the given location
     -- we should have at least found that agent...  */
  assert (num_aggressive_movers + num_passive_movers);

  if (num_aggressive_movers == 0)
    {
      if (num_passive_movers == 1)
        {
          /* A passive move with no competition can proceed.  */
          agent_t *agent = passive_movers->agent_data;
          perform_move (agent, pass_number * 10);
          remove_agent_list_head (&passive_movers);
          if ((number_agent_moves (agent) > pass_number) && (agent->energy > 0))
            add_agent_to_list (agent, next_pass_list);
        }
      else
        {
          /* Multiple passive movers all yield.  */
          stop_agents (pass_number, &passive_movers);
        }
    }
  else if (num_aggressive_movers == 1)
    {
      agent_t *agent = aggressive_movers->agent_data;

      /* Exactly one aggressive mover means all passive movers yield.  */
      stop_agents (pass_number, &passive_movers);
      perform_move (agent, pass_number * 10);
      remove_agent_list_head (&aggressive_movers);
      if ((number_agent_moves (agent) > pass_number) && (agent->energy > 0))
        add_agent_to_list (agent, next_pass_list);
    }
  else /* >1 aggressive movers */
    {
      /* More than one aggressive mover means all passive movers yield and
         all aggressive movers fight it out.  */
      stop_agents (pass_number, &passive_movers);
      resolve_move_fight (pass_number, &aggressive_movers, next_pass_list,
                          pass_number * 10);
    }

  assert (passive_movers == NULL);
  assert (aggressive_movers == NULL);
}

/* Find all moves in this_pass_list that target location [x,y].  Remove them from
   this_pass_list and place them into passive_movers or aggressive_movers, depending
   on the type of move that they are.  */
static void
calculate_movers_to_loc (long x, long y, agent_list_t **this_pass_list,
                         agent_list_t **passive_movers, unsigned int *num_passive_movers,
                         agent_list_t **aggressive_movers, unsigned int *num_aggressive_movers)
{
  while (*this_pass_list)
    {
      long facing_x, facing_y;
      int aggressive_p;
      agent_t *agent = (*this_pass_list)->agent_data;

      if (agent_is_moving (agent, &aggressive_p))
        {
          calculate_facing_location (agent, &facing_x, &facing_y);

          if ((facing_x == x) && (facing_y == y))
            {
              remove_agent_list_head (this_pass_list);
              if (aggressive_p)
                {
                  add_agent_to_list (agent, aggressive_movers);
                  (*num_aggressive_movers)++;
                }
              else
                {
                  add_agent_to_list (agent, passive_movers);
                  (*num_passive_movers)++;
                }
              continue;
            }
        }

      this_pass_list = &(*this_pass_list)->next;
    }
}

/* Execute the move of an agent, reducing their energy by the specified amount.  */
static void
perform_move (agent_t *agent, long energy_used)
{
  long facing_x, facing_y;

  calculate_facing_location (agent, &facing_x, &facing_y);
  assert (valid_location (facing_x, facing_y));
  assert (env.map[facing_y][facing_x].occ_type == OCC_EMPTY);
  update_agent_location (agent, facing_x, facing_y);
  modify_agent_energy (agent, -energy_used, EVENT_MOVED, 0);
}

/* Prevent all agents in the list from making any more moves this turn.  If
   they were unable to move at all, charge them the cost of a STAY action.  */
static void
stop_agents (unsigned int pass_number, agent_list_t **agent_list)
{
  while (*agent_list)
    {
      agent_t *agent = (*agent_list)->agent_data;

      if (pass_number == 1)
        process_action_stay (agent);

      remove_agent_list_head (agent_list);
    }
}

/* Handle the case where 2 or more agents are aggressively trying to move into
   the same location.  They will each be charged for a fight, and whichever
   one wins will also be charged the value of a move.  If the strongest two
   or more agents are tied on strength, neither of them will move.  */
static void
resolve_move_fight (unsigned pass_number, agent_list_t **aggressive_movers,
                    agent_list_t **next_pass_list, int move_energy)
{
  agent_t *strongest;
  int tie = 0;

  deduct_fight_cost (*aggressive_movers);
  strongest = (*aggressive_movers)->agent_data;
  remove_agent_list_head (aggressive_movers);

  while (*aggressive_movers)
    {
      agent_t *agent = (*aggressive_movers)->agent_data;

      if (agent->energy > strongest->energy)
        {
          strongest = agent;
          tie = 0;
        }
      else if (agent->energy == strongest->energy)
        tie = 1;

      remove_agent_list_head (aggressive_movers);
    }

  if (tie)
    return;

  if (strongest->energy > 0)
    {
      perform_move (strongest, move_energy);
      if (number_agent_moves (strongest) > pass_number)
        add_agent_to_list (strongest, next_pass_list);
    }
}

/* For each combination of pairs of aggressors, charge each aggressor the cost
   of a fight.  For example, with 3 aggressors we will charge:
   a1 vs. a2
   a1 vs. a3
   a2 vs. a3  */
static void
deduct_fight_cost (agent_list_t *aggressors)
{
  agent_list_t *agg1, *agg2;

  agg1 = aggressors;

  while (agg1)
    {
      agg2 = agg1->next;
      while (agg2)
        {
          modify_agent_energy (agg1->agent_data, -DEFAULT_FIGHT_COST, EVENT_FOUGHT,
                               agg2->agent_data->id);
          modify_agent_energy (agg2->agent_data, -DEFAULT_FIGHT_COST, EVENT_FOUGHT,
                               agg1->agent_data->id);
          agg2 = agg2->next;
        }
      agg1 = agg1->next;
    }
}

/* Process an EAT action.  */
static void
process_action_eat (agent_t *agent, agent_list_t **this_pass_list, int aggressive_p)
{
  long facing_x, facing_y;
  location_t *loc;
  veg_instance_t *veg_instance;

  agent_list_t *passive_eaters = NULL;
  unsigned int num_passive_eaters = 0;
  agent_list_t *aggressive_eaters = NULL;
  unsigned int num_aggressive_eaters = 0;

  calculate_facing_location (agent, &facing_x, &facing_y);
  loc = &env.map[facing_y][facing_x];
  if ((! valid_location (facing_x, facing_y))
      || (loc->occ_type != OCC_VEG))
    {
      /* Why are we trying to eat something that isn't vegetation?  */
      process_action_stay (agent);
      remove_agent_from_list (agent, this_pass_list);
      return;
    }

  veg_instance = &loc->veg;

  /* Find all agents who are trying to eat from this same location.  */
  calculate_eaters_of_loc (facing_x, facing_y, this_pass_list,
                           &passive_eaters, &num_passive_eaters,
                           &aggressive_eaters, &num_aggressive_eaters);

  assert (num_aggressive_eaters + num_passive_eaters);

  if (num_aggressive_eaters == 0)
    {
      /* If everybody is eating passively, we can just share the wealth.  */
      int energy_per_agent = (veg_bloom_value (veg_instance) + (num_passive_eaters - 1))
                             / num_passive_eaters;

      while (passive_eaters)
        {
          agent_t *agent = passive_eaters->agent_data;
          modify_agent_energy (agent, energy_per_agent, EVENT_ATE,
                               veg_instance->veg_info->id);
          modify_agent_energy (agent, -DEFAULT_EAT_COST, EVENT_UNLOGGED, 0);
          remove_agent_list_head (&passive_eaters);
        }

      /* The vegetation has to start its life cycle over.  */
      reset_veg_lifecycle (veg_instance);
    }
  else if (num_aggressive_eaters == 1)
    {
      /* A lone aggressive agent gets all of the nourishment.  */
      agent_t *agent = aggressive_eaters->agent_data;
      modify_agent_energy (agent, veg_bloom_value (veg_instance), EVENT_ATE,
                           veg_instance->veg_info->id);
      modify_agent_energy (agent, -DEFAULT_EAT_COST, EVENT_UNLOGGED, 0);
      remove_agent_list_head (&aggressive_eaters);

      while (passive_eaters)
        {
          /* Passive eaters who yielded get no nourishment, and get charged for
             the cost of eating.  */
          modify_agent_energy (passive_eaters->agent_data, -DEFAULT_EAT_COST,
                               EVENT_UNLOGGED, 0);
          remove_agent_list_head (&passive_eaters);
        }
      reset_veg_lifecycle (veg_instance);
    }
  else
    {
      /* More than 1 aggressive eater - fight, fight, fight!   */
      while (passive_eaters)
        {
          /* Passive eaters yield.  */
          modify_agent_energy (passive_eaters->agent_data, -DEFAULT_EAT_COST,
                               EVENT_UNLOGGED, 0);
          remove_agent_list_head (&passive_eaters);
        }
      resolve_food_fight (&aggressive_eaters, veg_instance);
    }

  assert (passive_eaters == NULL);
  assert (aggressive_eaters == NULL);
}

/* Find all agents in this_pass_list who are eating from the location [x,y]
   and move them into two lists -- passive_eaters and aggressive_eaters.  */
static void
calculate_eaters_of_loc (long x, long y, agent_list_t **this_pass_list,
                         agent_list_t **passive_eaters,
                         unsigned int *num_passive_eaters,
                         agent_list_t **aggressive_eaters,
                         unsigned int *num_aggressive_eaters)
{
  long facing_x, facing_y;

  while (*this_pass_list)
    {
      int aggressive_p;
      agent_t *agent = (*this_pass_list)->agent_data;

      if (agent->action == ACTION_EAT_PASSIVE)
        aggressive_p = 0;
      else if (agent->action == ACTION_EAT_AGGRESSIVE)
        aggressive_p = 1;
      else
        {
          this_pass_list = &(*this_pass_list)->next;
          continue;
        }

      calculate_facing_location (agent, &facing_x, &facing_y);

      if ((facing_x == x) && (facing_y == y))
        {
          remove_agent_list_head (this_pass_list);
          if (aggressive_p)
            {
              add_agent_to_list (agent, aggressive_eaters);
              (*num_aggressive_eaters)++;
            }
          else
            {
              add_agent_to_list (agent, passive_eaters);
              (*num_passive_eaters)++;
            }
        }
      else
        this_pass_list = &(*this_pass_list)->next;
    }
}

/* Handle a list of aggressive_eaters trying to feed on the same vegetation.  They
   will each be charged for a fight, and the strongest will get all of the
   nourishment.  If there is a tie for the strongest, nobody will get any
   nourishment.  */
static void
resolve_food_fight (agent_list_t **aggressive_eaters, veg_instance_t *veg_instance)
{
  agent_t *strongest;
  int tie = 0;

  deduct_fight_cost (*aggressive_eaters);
  strongest = (*aggressive_eaters)->agent_data;
  remove_agent_list_head (aggressive_eaters);

  while (*aggressive_eaters)
    {
      agent_t *agent = (*aggressive_eaters)->agent_data;

      if (agent->energy > strongest->energy)
        {
          strongest = agent;
          tie = 0;
        }
      else if (agent->energy == strongest->energy)
        tie = 1;

      remove_agent_list_head (aggressive_eaters);
    }

  if (tie)
    return;

  if (strongest->energy > 0)
    {
      modify_agent_energy (strongest, veg_bloom_value (veg_instance),
                           EVENT_ATE, veg_instance->veg_info->id);
      reset_veg_lifecycle (veg_instance);
    }
}

/* Free all instances of predators from the current environment.  */
void
destroy_all_predators (void)
{
  while (env.all_predators)
    {
      pred_instance_t *head = env.all_predators;
      env.all_predators = env.all_predators->next;
      free (head);
    }
} 

/* Get rid of our current environment.  */
void
destroy_environment (void)
{
  LOG ((LOG_SCREEN | LOG_FILE), "\nDestroying world\n");

  while (env.types_of_vegetation != NULL)
    {
      vegetation_list_t *next;

      next = env.types_of_vegetation->next;
      free (env.types_of_vegetation->bloom_schedule.bloom_table);
      free (env.types_of_vegetation);
      env.types_of_vegetation = next;
    }

  while (env.types_of_predators != NULL)
    {
      pred_type_t *next;

      next = env.types_of_predators->next;
      free (env.types_of_predators);
      env.types_of_predators = next;
    }
}

