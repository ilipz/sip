#ifndef RX_MSG_H
#define RX_MSG_H

#include "../types.h"
extern struct global_var g;
extern numrecord_t nums[10];
pj_bool_t logger_rx_msg(pjsip_rx_data *rdata);

#endif
