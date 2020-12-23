#include "free.h"
extern struct global_var g;
void free_junction (junction_t *j)
{
    const char *THIS_FUNCTION = "free_junction()";
    pj_status_t status;
    char FULL_INFO[64];
    if (j == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"gotten empty junc pointer"));
        return;
    }

    
    sprintf (FULL_INFO, "%s for junc#%d", THIS_FUNCTION, j->index);

    if (j->state == READY)
        return;

    if ( (status = pj_mutex_trylock (j->mutex)) != PJ_SUCCESS)
    {
        if (status != PJ_EBUSY)
            pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pj_mutex_trylock()");
        return; 
    }

    if (j->state == DISABLED)
    {
        PJ_LOG (5, (FULL_INFO, PJ_LOG_ERROR"junc disabled"));
        return;
    }

    if (j->out_leg.current.stream_conf_id < 32 && j->in_leg.current.stream_conf_id < 32) 
    {
        // TODO: Move the disconnects and removes in free_leg (so if conf_id==32 then don't disconnect/remove)
        status = pjmedia_conf_disconnect_port (g.conf, j->in_leg.current.stream_conf_id, j->out_leg.current.stream_conf_id); // CATCH
        if (status != PJ_SUCCESS)
            emergency_exit ("conf disconnect in-out port",  &status);

        status = pjmedia_conf_disconnect_port (g.conf, j->out_leg.current.stream_conf_id, j->in_leg.current.stream_conf_id); // CATCH
        if (status != PJ_SUCCESS)
            emergency_exit ("conf disconnect out-in port",  &status);

        status = pjmedia_conf_remove_port (g.conf, j->out_leg.current.stream_conf_id); // CATCH
        if (status != PJ_SUCCESS)
            emergency_exit ("conf remove out port",  &status);

        status = pjmedia_conf_remove_port (g.conf, j->in_leg.current.stream_conf_id); // CATCH
        if (status != PJ_SUCCESS)
            emergency_exit ("conf remove in port",  &status);
    }
    

    free_leg (&j->out_leg);
    free_leg (&j->in_leg);

    pj_mutex_unlock (j->mutex); // CATCH

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

    pjmedia_transport_media_stop (l->media_transport);
    pjmedia_transport_detach (l->media_transport, NULL);
    if (l->current.stream)
    {
        status = pjmedia_stream_destroy (l->current.stream); // CATCH
        if (status != PJ_SUCCESS)
            emergency_exit ("stream destroy", &status);
    }
    else
    {
        halt ("l->current stream");
    }
    
        
    if (l->current.inv)
    {
        if (l->current.inv->state != PJSIP_INV_STATE_DISCONNECTED)
        {
            status = pjsip_inv_end_session (l->current.inv, 200, NULL, &tdata);
            if (l->type == IN)
                if (tdata)
                {
                    tdata->via_addr.host = g.local_addr;
                    tdata->via_addr.port = g.sip_port;
                }
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
            PJ_LOG (5, (FULL_INFO, PJ_LOG_ERROR"inv state is DISCONNECTED. No terminate msg created"));        
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
    l->current.stream_conf_id = 32;

}

void destroy_junction (junction_t *j)
{
    if (j == NULL)
    {
        return;
    }

    if (j->state == DISABLED)
        return;
    
    //if (pj_mutex_trylock (j->mutex) != PJ_SUCCESS) return;

    j->state = DISABLED;

    //pj_mutex_unlock (j->mutex); //CATCH
    pj_mutex_destroy (j->mutex); //CATCH
    

    pjmedia_transport_close (j->out_leg.media_transport); //CATCH

    pjmedia_transport_close (j->in_leg.media_transport); //CATCH

}