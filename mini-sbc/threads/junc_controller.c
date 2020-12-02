#include "junc_controller.h"

extern struct global_var g;

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
            pjmedia_master_port_set_dport (j->mp_out_in, j->in_leg.current.stream_port);
            pjmedia_master_port_start (j->mp_in_out);
            return 0;
        }
        //pjsip_dlg_modify_response

        if (j->in_leg.current.inv)
        switch (j->in_leg.current.inv->state)
        {
            case PJSIP_INV_STATE_DISCONNECTED:
            case PJSIP_INV_STATE_NULL:
                return 0;
            default: break;
        }

        if (j->out_leg.current.inv)
        switch (j->out_leg.current.inv->state)
        {
            case PJSIP_INV_STATE_DISCONNECTED:
            case PJSIP_INV_STATE_NULL:
                return 0;
            default: break;
        }
        if (g.to_quit)
            return 0;
    }
    return 0;
}
