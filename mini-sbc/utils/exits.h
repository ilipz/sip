#ifndef EXITS_H
#define EXITS_H
#include <stdlib.h>
#include "../types.h"
#include "../juncs/free.h"

void emergency_exit (char *sender, pj_status_t *status);
void halt (char *sender);

#endif
