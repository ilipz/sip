#ifndef UTIL_H
#define UTIL_H

#include "../types.h"
#include "exits.h"
extern numrecord_t nums[10];

pj_bool_t   create_sdp     (pj_pool_t *pool, leg_t *l, pjmedia_sdp_session **p_sdp);
pj_bool_t   make_call      (numrecord_t *tel, leg_t *l);
numrecord_t *get_numrecord (char *num);

#endif
