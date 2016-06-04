#ifndef __VEGETATION_H__
#define __VEGETATION_H__

#include "environment.h"

unsigned int veg_lifecycle_len (vegetation_list_t *veg_info);
int veg_bloom_value (veg_instance_t *veg_instance);
void reset_veg_lifecycle (veg_instance_t *veg_instance);
 
#endif /* ! defined (__VEGETATION_H__) */
