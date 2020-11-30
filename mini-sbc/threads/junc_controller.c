#include "junc_controller.h"

int junc_controller (void *p)
{
    junction_t *j = (junction_t*) p;
    while (1)
    {
        if (j->out_leg.current.sdp_neg_done == PJ_TRUE && j->in_leg.current.sdp_neg_done == PJ_TRUE)
        {
            pjmedia_master_port_set_uport (j->mp_in_out, j->in_leg.current.stream_port);
            pjmedia_master_port_set_dport (j->mp_in_out, j->out_leg.current.stream_port);
            pjmedia_master_port_start (j->mp_in_out);

            pjmedia_master_port_set_uport (j->mp_out_in, j->out_leg.current.stream_port);
            pjmedia_master_port_set_dport (j->mp_in_out, j->in_leg.current.stream_port);
            pjmedia_master_port_start (j->mp_in_out);
            break;
        }

        switch (j->in_leg.current.inv->state)
        {
            case PJSIP_INV_STATE_DISCONNECTED:
            case PJSIP_INV_STATE_NULL:
                break;
            default: continue;
        }

        switch (j->out_leg.current.inv->state)
        {
            case PJSIP_INV_STATE_DISCONNECTED:
            case PJSIP_INV_STATE_NULL:
                break;
            default: continue;
        }
        if (g.to_quit)
            return 0;
    }
    return 0;
}
