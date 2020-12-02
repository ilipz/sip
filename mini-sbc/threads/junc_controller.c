#include "junc_controller.h"

extern struct global_var g;

int junc_controller (void *p)
{
    junction_t *j = (junction_t*) p;
    pj_status_t status;
    printf ("\n\n\nENTERD JUNC CONTROLLER\n\n\n\n");
    while (1)
    {
        if (1)//j->out_leg.current.sdp_neg_done == PJ_TRUE && j->in_leg.current.sdp_neg_done == PJ_TRUE)
        {
            /*status =pjmedia_master_port_set_uport (j->mp_in_out, j->in_leg.current.stream_port);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, "jnc", status, "master_port30");
                exit (8);
            }
            status =pjmedia_master_port_set_dport (j->mp_in_out, j->out_leg.current.stream_port);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, "jnc", status, "master_port20");
                exit (8);
            }
            status =pjmedia_master_port_start (j->mp_in_out); 
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, "jnc", status, "master_port10");
                exit (8);
            }
            sleep(3);
            printf ("\n\n\n\nMASTER PORTS\n\n\n\n"); */

            
            sleep(4);
            status = pjmedia_master_port_set_uport (j->mp_out_in, j->out_leg.current.stream_port);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, "jnc", status, "master_port1");
                exit (80);
            }
            status = pjmedia_master_port_set_dport (j->mp_out_in, j->in_leg.current.stream_port);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, "jnc", status, "master_port2");
                exit (80);
            }
            status = pjmedia_master_port_start (j->mp_out_in);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, "jnc", status, "master_port3");
                exit (80);
            }
            printf ("\n\n\nGONNA EXIT MPs\n\n\n\n"); 
            return 0;
        }
        //pjsip_dlg_modify_response

        if (j->in_leg.current.inv)
        switch (j->in_leg.current.inv->state)
        {
            case PJSIP_INV_STATE_DISCONNECTED:
            case PJSIP_INV_STATE_NULL:
                printf ("\n\n\nGONNA EXIT SWITCH 1\n\n\n\n");
                return 0;
            default: break;
        }

        if (j->out_leg.current.inv)
        switch (j->out_leg.current.inv->state)
        {
            case PJSIP_INV_STATE_DISCONNECTED:
            case PJSIP_INV_STATE_NULL:
                printf ("\n\n\nGONNA EXIT SWITCH 2\n\n\n\n");
                return 0;
            default: break;
        }
        if (g.to_quit)
            return 0;
    }
    printf ("\n\n\nGONNA EXIT FINISH\n\n\n\n");
    return 0;
}
