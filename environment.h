#ifndef __ENVIRONMENT_H__
#define __ENVIRONMENT_H__

#include <sys/dir.h>
#include "agent.h"

/* Vegetation will incubate for the specified incubation time, and then
   bloom according to the bloom table.  */
typedef struct {
  unsigned int incubation_time;
  unsigned int bloom_table_size;
  int *bloom_table;
} bloom_schedule_t;

/* Represents a single species of vegetation.  "next" is because all species are
   maintained in a linked list in the environment.  */
typedef struct vegetation_list {
  char *name;
  unsigned int id;
  float frequency;
  bloom_schedule_t bloom_schedule;
  struct vegetation_list *next;
} vegetation_list_t;

/* Represents a single species of predator.  "next" is because all species are
   maintained in a linked list in the environment.  */
typedef struct pred_type {
  char *name;
  unsigned int id;
  unsigned int damage;
  unsigned int life_span;
  unsigned int num_instances;
  struct pred_type *next;
} pred_type_t;

enum occupant {
  OCC_EMPTY,
  OCC_AGENT,
  OCC_PRED,
  OCC_VEG
};

/* An instance of a predator.  Maintains all of the information needed for this
   unique creature, as well as a pointer (pred_info) to the general species
   information.  */
typedef struct pred_instance {
  pred_type_t *pred_info;

  /* Relative age of the animal (age at time 0).  This allows us to calculate
     the animal's age at any time without having to increment its each each
     turn.  */
  int lifecycle_offset;

  unsigned int x_pos;
  unsigned int y_pos;
  struct pred_instance *next;
} pred_instance_t;

/* Information for a single instance of vegetation.  */
typedef struct {
  vegetation_list_t *veg_info;
  int lifecycle_offset;
} veg_instance_t;

/* Tells us what is in each space on the map.  */
typedef struct location {
  enum occupant occ_type;
  union {
    agent_t *agent;
    pred_instance_t *pred;
    veg_instance_t veg;
  };
} location_t;

struct environment_t {

  /* How far into the simulation we are (number of turns).  */
  unsigned int current_time;

  /* How long this simulation will last (number of turns).  */
  unsigned int simulation_length;

  /* Size of the environment ("playing field").  */
  unsigned int size_x;
  unsigned int size_y;

  /* How much energy each agent will start with.  */
  unsigned int default_energy;

  /* How many squares the agent can see in front of themselves.  */
  unsigned int visibility;

  /* A list of all agents that are still alive in the environment.  */
  agent_list_t *alive_agents;

  /* A list of all species of plants that can exist in this environment.  */
  vegetation_list_t *types_of_vegetation;

  /* A list of all species of predators that can exist in this environment.  */
  pred_type_t *types_of_predators;

  /* A list of all instances of predators in the environment.  */
  pred_instance_t *all_predators;

  /* The map containing information about what is in each square of the
     environment.  */
  location_t **map;
};

#define DEFAULT_STAY_COST 1
#define DEFAULT_TURN_COST 2
#define DEFAULT_EAT_COST 5
#define DEFAULT_FIGHT_COST 30

#define AGENT_VISIBLE_DISTANCE 5
#define PREDATOR_VISIBLE_DISTANCE 4

extern struct environment_t env;

/* These are all of the actions that have not yet been processed for this phase of the
   current turn.  */
extern agent_list_t *this_pass_actions;

int initialize_from_env_file (struct dirent *env_file);
void regenerate_predators (void);
void gen_percept_at_distance (char **str_ptr, agent_t *agent, unsigned int distance);
void resolve_agent_actions (void);
void resolve_predator_actions (void);
void destroy_all_predators (void);
void destroy_environment (void);

#endif /* defined (__ENVIRONMENT_H__) */

