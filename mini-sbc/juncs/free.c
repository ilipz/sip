#include "free.h"
extern struct global_var g;
void free_junction (junction_t *j)
{
    if (j->state == DISABLED)
        return;

    pjmedia_master_port_stop (j->mp_in_out);
    pjmedia_master_port_stop (j->mp_out_in);
    pj_thread_destroy (j->controller_thread);
    // if failed then disable junction
    free_leg (&j->out_leg);
    free_leg (&j->in_leg);
    j->state = READY;
    

}

void free_leg (leg_t *l)
{
    if (l == NULL)
        return;
    pjsip_tx_data *tdata;
    int status_code=0;
    pj_status_t status;
   // TODO:
   // lock & unlock mutex
   // disconnect&remove from tonegen conference
   // destroy stream
    /*switch (l->current.inv->state)
    {
        case PJSIP_INV_STATE_DISCONNECTED: 
        case PJSIP_INV_STATE_CONFIRMED:
        case PJSIP_INV_STATE_CONNECTING:
            status_code = 200; break; 
        
        case PJSIP_INV_STATE_CALLING:
        case PJSIP_INV_STATE_INCOMING:
        case PJSIP_INV_STATE_EARLY:
            status_code = 603; break;
        
        default:
            break;
        
    }
    if (status_code > 0)
    {*/
    if (l->current.inv)
    if (l->current.inv->state != PJSIP_INV_STATE_DISCONNECTED)
    {
        status = pjsip_inv_end_session (l->current.inv, 200, NULL, &tdata);
        if (status != PJ_SUCCESS)
            pj_perror (5, "free_leg", status, "inv end session");
        if (tdata)
            pjsip_inv_send_msg (l->current.inv, tdata);
    }
    
    //}
    
    nullize_leg (l);
}

void nullize_leg (leg_t *l)
{
    if (l == NULL)
        return;
    l->current.inv = NULL;
    l->current.local_sdp = NULL;
    l->current.remote_sdp = NULL;
    //l->current.rtcp_session = NULL;
    l->current.stream = NULL;
    l->current.stream_info = NULL;
    l->current.stream_port = NULL;
    l->current.sdp_neg_done = PJ_FALSE;
}