#ifndef FREE_H
#define FREE_H

#include "../types.h"
#include "../utils/exits.h"


void free_junction    (junction_t *j);
void free_leg         (leg_t *l);
void nullize_leg      (leg_t *l);
void destroy_junction (junction_t *j);

#endif
