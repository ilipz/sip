#include "free.h"
extern struct global_var g;
void free_junction (junction_t *j)
{
    const char *THIS_FUNCTION = "free_junction()";

    if (j == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"gotten empty junc pointer"));
        return;
    }

    char FULL_INFO[64];
    sprintf (FULL_INFO, "%s for junc#%d", THIS_FUNCTION, j->index);
    
    if (j->state == DISABLED)
    {
        PJ_LOG (5, (FULL_INFO, PJ_LOG_ERROR"junc disabled"));
        return;
    }

    pj_status_t status;

    status = pjmedia_master_port_stop (j->mp_in_out);
    if (status != PJ_SUCCESS)
    {
        pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjmedia_master_port_stop() for mp_in_out. Junc will disabled");
        j->state = DISABLED;
    }
        

    status = pjmedia_master_port_stop (j->mp_out_in);
    if (status != PJ_SUCCESS)
    {
        pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjmedia_master_port_stop() for mp_out_in. Junc will disabled");
        j->state = DISABLED;
    }

    /*status = pj_thread_destroy (j->controller_thread);
    if (status != PJ_SUCCESS)
    {
        pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pj_thread_destroy() for junc controller. Junc will disabled");
        j->state = DISABLED;
    } */
    // if failed then disable junction
    free_leg (&j->out_leg);
    free_leg (&j->in_leg);
    
    if (j->state != DISABLED) 
        j->state = READY;
}

void free_leg (leg_t *l)
{
    const char *THIS_FUNCTION = "free_leg()";

    if (l == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"gotten empty leg pointer"));
        return;
    }

    char FULL_INFO[64];
    sprintf (FULL_INFO, "%s for '%s' leg in junc#%d", THIS_FUNCTION, l->type == IN ? "IN" : "OUT", l->junction_index);

    PJ_LOG (5, (FULL_INFO, "Enterence"));

    pjsip_tx_data *tdata=NULL;;
    pj_status_t status;
    if (l->current.stream)
        pjmedia_stream_destroy (l->current.stream);
    if (l->current.inv)
    {
        if (l->current.inv->state != PJSIP_INV_STATE_DISCONNECTED)
        {
            status = pjsip_inv_end_session (l->current.inv, 200, NULL, &tdata);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjsip_inv_end_session()");

            }
            else 
            {
                if (tdata)
                {
                    status = pjsip_inv_send_msg (l->current.inv, tdata);
                    if (status != PJ_SUCCESS)
                        pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjsip_inv_send_msg()");
                }  
                else
                    PJ_LOG (5, (FULL_INFO, PJ_LOG_ERROR"gotten empty tdata"));
            }
        }
        else
            PJ_LOG (5, (FULL_INFO, "inv state is DISCONNECTED. No terminate msg created"));        
    }
    else
        PJ_LOG (5, (FULL_INFO, PJ_LOG_ERROR"gotten empty leg inv pointer (l->current.inv ==NULL)"));
    nullize_leg (l);
    PJ_LOG (5, (FULL_INFO, "Exit"));
}

void nullize_leg (leg_t *l)
{
    const char *THIS_FUNCTION = "nullize_leg()";

    if (l == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, "Gotten empty leg pointer"));
        return;
    }
    
    PJ_LOG (5, (THIS_FUNCTION, "Nullize %s leg in junc#%d", l->type==IN ? "IN" : "OUT", l->junction_index));

    l->current.inv = NULL;
    l->current.local_sdp = NULL;
    l->current.remote_sdp = NULL;
    l->current.stream = NULL;
    l->current.stream_info = NULL;
    l->current.stream_port = NULL;
    l->current.sdp_neg_done = PJ_FALSE;
}

void destroy_junction (junction_t *j)
{
    if (j == NULL)
    {

        return;
    }

    if (j->state == DISABLED)
        return;
    
    if (pj_mutex_trylock (j->mutex) != PJ_SUCCESS)
    {

        return;
    }

    j->state = DISABLED;

    pjmedia_master_port_destroy (j->mp_in_out, PJ_FALSE);

    pjmedia_master_port_destroy (j->mp_out_in, PJ_FALSE);

    pjmedia_transport_close (j->out_leg.media_transport);

    pjmedia_transport_close (j->in_leg.media_transport);

}