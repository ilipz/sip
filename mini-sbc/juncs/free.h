#ifndef FREE_H
#define FREE_H

#include "junc_t.h"
#include "../types.h"



void free_junction (junction_t *j);
void free_leg (leg_t *l);
void nullize_leg (leg_t *l);
void destroy_junction (junction_t *j);

#endif
