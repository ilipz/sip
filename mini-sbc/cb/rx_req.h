#ifndef RX_REQ_H
#define RX_REQ_H

#include "../types.h"
#include "../juncs/junc_t.h"
#include "../utils/inits.h"
#include "../utils/util.h"
#include "../threads/junc_controller.h"

pj_bool_t on_rx_request (pjsip_rx_data *rdata);

#endif
