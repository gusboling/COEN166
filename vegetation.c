#include "environment.h"
#include "vegetation.h"

/* Returns the length of a full life cycle for a vegetation species.
   This value includes both the incubation time and the bloom time.  */
unsigned int
veg_lifecycle_len (vegetation_list_t *veg_info)
{
  return veg_info->bloom_schedule.incubation_time
         + veg_info->bloom_schedule.bloom_table_size;
}

/* Determine the bloom value of vegetation (the current energy it has
   to offer).  This value is based on how far it is into its life
   cycle.  */
int
veg_bloom_value (veg_instance_t *veg_instance)
{
  vegetation_list_t *veg_info = veg_instance->veg_info;
  unsigned cycle_offset = (veg_instance->lifecycle_offset + env.current_time)
                          % veg_lifecycle_len (veg_info);
  unsigned bloom_offset;

  if (cycle_offset < veg_info->bloom_schedule.incubation_time)
    return 0;

  bloom_offset = cycle_offset - veg_info->bloom_schedule.incubation_time;

  return veg_info->bloom_schedule.bloom_table[bloom_offset];
}

/* Restart a plant at the beginning of its life cycle.  Note that this
   isn't quite as easy as it might be, since we only store the plant's
   age relative to the beginning of the simulation.  So, we have to
   calculate a value for its life cycle offset such that:
     (offset + current time) % total life cycle length == 0  */
void
reset_veg_lifecycle (veg_instance_t *veg_instance)
{
  unsigned int lifecycle_len = veg_lifecycle_len (veg_instance->veg_info);

  if (lifecycle_len == 0)
    return;

  veg_instance->lifecycle_offset = (lifecycle_len - 1)
                                   - (env.current_time % lifecycle_len);
}

