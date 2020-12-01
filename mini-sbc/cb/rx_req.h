#ifndef _RX_REQ_H
#define _RX_REQ_H

#include "../types.h"

#include "../utils/inits.h"
#include "../utils/util.h"
#include "../threads/junc_controller.h"

pj_bool_t on_rx_request (pjsip_rx_data *rdata);

#endif