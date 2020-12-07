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

    while (j->out_leg.current.inv->state != PJSIP_INV_STATE_CONFIRMED); // INVALID
    
    pjsip_tx_data *tdata;
    status = pjsip_inv_answer(j->in_leg.current.inv,  200, NULL, NULL, &tdata);
    if (status != PJ_SUCCESS)
            halt ("rx_req.c");
    
    status = pjsip_inv_send_msg(j->in_leg.current.inv, tdata);
     if (status != PJ_SUCCESS)
            halt ("rx_req.c");
    
    while (1)
    {
        if (j->out_leg.current.inv && j->in_leg.current.inv) 
        if (j->out_leg.current.inv->state == PJSIP_INV_STATE_CONFIRMED && j->in_leg.current.inv->state == PJSIP_INV_STATE_CONFIRMED)
        {
            
            PJ_LOG (5, (FULL_INFO, "Starting master port connection"));
            status = pjmedia_master_port_set_uport (j->mp_in_out, j->in_leg.current.stream_port);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjmedia_master_port_set_uport() for mp_in_out. Junc disabled");
                j->state = DISABLED;
                return 0;      
            }

            status = pjmedia_master_port_set_dport (j->mp_in_out, j->out_leg.current.stream_port);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjmedia_master_port_set_dport() for mp_in_out. Junc disabled");
                j->state = DISABLED;
                return 0;      
            }

            

            status = pjmedia_master_port_start (j->mp_in_out); 
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjmedia_master_port_start() for mp_in_out. Junc disabled");
                j->state = DISABLED;
                return 0;      
            }
//return 0;
            status = pjmedia_master_port_set_uport (j->mp_out_in, j->out_leg.current.stream_port);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjmedia_master_port_set_uport() for mp_out_in. Junc disabled");
                j->state = DISABLED;
                return 0;      
            }

            status = pjmedia_master_port_set_dport (j->mp_out_in, j->in_leg.current.stream_port);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjmedia_master_port_set_dport() for mp_out_in. Junc disabled");
                j->state = DISABLED;
                return 0;      
            }

            status = pjmedia_master_port_start (j->mp_out_in);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjmedia_master_port_start() for mp_out_in. Junc disabled");
                j->state = DISABLED;
                return 0;      
            }

            PJ_LOG (5, (FULL_INFO, "Junction master ports started. Job done. Exit"));
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
    }

    printf ("\n\n\n(WARNING) %s: OUT OF LOOP EXIT\n\n\n\n", FULL_INFO);
    return 0;
}
