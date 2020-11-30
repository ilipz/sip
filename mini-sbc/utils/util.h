#ifndef UTIL_H
#define UTIL_H

pj_bool_t create_sdp( pj_pool_t *pool, leg_t *l, pjmedia_sdp_session **p_sdp);
pj_bool_t make_call(numrecord_t *tel, leg_t *l);
char num_addr (char *num);

#endif
