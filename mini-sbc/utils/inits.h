#ifndef INITS_H
#define INITS_H

#include "../types.h"
#include "../juncs/free.h"
#include "../loggers/rx_msg.h"
#include "../loggers/tx_msg.h"
#include "../cb/rx_req.h"
#include "../cb/media_upd.h"
#include "../cb/state_chd.h"
#include "../cb/forked.h"

void      init_sip ();
pj_bool_t init_media ();
void      init_juncs ();
void      init_exits ();
#endif
