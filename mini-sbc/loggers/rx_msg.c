#include "rx_msg.h"
extern struct global_var g;
pj_bool_t logger_rx_msg(pjsip_rx_data *rdata)
{
	return PJ_FALSE;
    PJ_LOG(4,(APPNAME, "RX %d bytes %s from %s:%d:\n"
			 "%s\n"
			 "--end msg--",
			 rdata->msg_info.len,
			 pjsip_rx_data_get_info(rdata),
			 rdata->pkt_info.src_name,
			 rdata->pkt_info.src_port,
			 rdata->msg_info.msg_buf));
    
    /* Always return false, otherwise messages will not get processed! */
    return PJ_FALSE;
}
