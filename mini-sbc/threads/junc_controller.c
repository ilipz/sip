#include "junc_controller.h"

extern struct global_var g;

int junc_controller (void *p)
{
    const static char *THIS_FUNCTION = "junc_controller()";
    junction_t *j = (junction_t*) p;
    if (j == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"junc pointer == NULL. Quiting..."));
        return 1;
    }
    
    static char FULL_INFO[64];
    sprintf (FULL_INFO, "%s thread for junc#%d", THIS_FUNCTION, j->index);
    
    PJ_LOG (5, (FULL_INFO, "Enterence"));

    pj_status_t status;

    while (!j->out_leg.current.inv) // waiting for out leg inv session to create
    {
        if (j->in_leg.current.inv)
            if (j->in_leg.current.inv->state == PJSIP_INV_STATE_DISCONNECTED || j->in_leg.current.inv->state == PJSIP_INV_STATE_NULL)
            {
                PJ_LOG (5, (FULL_INFO, "Exit due to IN leg invite state (DISSCONECTED or NULL) before OUT leg confirmed"));
                return 0;
            }
        
        status = pj_thread_sleep (500);
        if (status != PJ_SUCCESS)
            emergency_exit (FULL_INFO, &status); 
    }

    // 
    if (j->in_leg.current.inv->state == PJSIP_INV_STATE_DISCONNECTED || j->in_leg.current.inv->state == PJSIP_INV_STATE_NULL)
    {
        PJ_LOG (5, (FULL_INFO, "Exit due to IN leg invite state (DISSCONECTED or NULL) before OUT leg confirmed"));
        return 0;
    }
    else if (j->out_leg.current.inv->state == PJSIP_INV_STATE_DISCONNECTED || j->out_leg.current.inv->state == PJSIP_INV_STATE_NULL)
    {
        PJ_LOG (5, (FULL_INFO, "Exit due to OUT leg invite state (DISSCONECTED or NULL)"));
        pjsip_tx_data *tdata;
        //printf ("\n\n\n\nALLALALALAL\n\n\n\n");
        sleep (5);
        status = pjsip_inv_answer(j->in_leg.current.inv,  603, NULL, NULL, &tdata);
        if (status != PJ_SUCCESS)
            emergency_exit (FULL_INFO, &status);

        status = pjsip_inv_send_msg(j->in_leg.current.inv, tdata);
        if (status != PJ_SUCCESS)
            emergency_exit (FULL_INFO, &status);
        return 0;
    } 
    else 
    {
        pjsip_tx_data *tdata;

        status = pjsip_inv_answer(j->in_leg.current.inv,  200, NULL, NULL, &tdata);
        if (status != PJ_SUCCESS)
            emergency_exit (FULL_INFO, &status);
    
        status = pjsip_inv_send_msg(j->in_leg.current.inv, tdata);
        if (status != PJ_SUCCESS)
            emergency_exit (FULL_INFO, &status);
    } 
    
    
    
    while (1)
    {
        if ( (j->out_leg.current.sdp_neg_done == PJ_TRUE) && (j->in_leg.current.sdp_neg_done == PJ_TRUE) ) 
            if (j->out_leg.current.inv->state == PJSIP_INV_STATE_CONFIRMED && j->in_leg.current.inv->state == PJSIP_INV_STATE_CONFIRMED)
            {
                PJ_LOG (5, (FULL_INFO, "Start connecting legs in conference bridge"));

                status = pjmedia_conf_connect_port (g.conf, j->out_leg.current.stream_conf_id, j->in_leg.current.stream_conf_id, 128);
                if (status != PJ_SUCCESS)
                    emergency_exit (FULL_INFO, &status);   

                status = pjmedia_conf_connect_port (g.conf, j->in_leg.current.stream_conf_id, j->out_leg.current.stream_conf_id, 128);
                if (status != PJ_SUCCESS)
                    emergency_exit (FULL_INFO, &status);

                /*status = pjmedia_stream_start(j->in_leg.current.stream);
                if (PJ_SUCCESS != status)
                {
                    pj_perror (5, FULL_INFO, status, "pjmedia_stream_start()");
                    return 0;
                } 

                status = pjmedia_stream_start(j->out_leg.current.stream);
                if (PJ_SUCCESS != status)
                {
                    pj_perror (5, FULL_INFO, status, "pjmedia_stream_start()");
                    return 0;
                } */

                PJ_LOG (5, (FULL_INFO, "Finish connecting legs in conference bridge. Exit"));
                
                return 0;
            }
        

        if (j->in_leg.current.inv)
            switch (j->in_leg.current.inv->state)
            {
                case PJSIP_INV_STATE_DISCONNECTED:
                case PJSIP_INV_STATE_NULL:
                    PJ_LOG (5, (FULL_INFO, "Exit due to in_leg invite state"));
                    return 0;
                default: break;
            }
        else
        {
            PJ_LOG (5, (FULL_INFO, "Exit due to cann't read in_leg invite state"));
            return 0;
        }
        
        if (j->out_leg.current.inv)
            switch (j->out_leg.current.inv->state)
            {
                case PJSIP_INV_STATE_DISCONNECTED:
                case PJSIP_INV_STATE_NULL:
                    PJ_LOG (5, (FULL_INFO, "Exit due to out_leg invite state"));
                    return 0;
                default: break;
            }
        else
        {
            PJ_LOG (5, (FULL_INFO, "Exit due to cann't read out_leg invite state"));
            return 0;
        }
        
        if (g.to_quit)
        {
            PJ_LOG (5, (FULL_INFO, "Global application exit"));
            return 0; 
        }

        status = pj_thread_sleep (500);
        if (status != PJ_SUCCESS)
            emergency_exit (FULL_INFO, &status); 
    }
}
