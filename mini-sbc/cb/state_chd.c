#include "state_chd.h"

extern struct global_var g;


void on_state_changed (pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(e);
    pj_status_t status;
    const char THIS_FUNCTION[] = "call_on_state_changed()";
    if (inv == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"inv pointer is empty"));
        return;
    }
    
    
    leg_t *l = inv->mod_data[g.mod_app.id];
    if (l == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, "Module data isn't defined. Cann't get leg pointer"));
        return;
    }

    char FULL_INFO[64];
    sprintf (FULL_INFO, "%s with %s leg in junc#%u", THIS_FUNCTION, l->type == IN ? "IN" : "OUT", l->junction_index);
    
    if (l->type == OUT)
        if (l->current.inv)
            if (l->current.inv->state == PJSIP_INV_STATE_CONFIRMED)
            {
                pjsip_tx_data *tdata;
                
                status = pjsip_inv_answer(l->reverse->current.inv, 200, NULL, NULL, &tdata);
                if (status != PJ_SUCCESS)
                {
                    pj_perror (5, FULL_INFO, status, "pjsip_inv_answer() with 200");
                    free_leg (l);
                    return;
                }

                status = pjsip_inv_send_msg(l->reverse->current.inv, tdata);
                if (status != PJ_SUCCESS)
                {
                    pj_perror (5, FULL_INFO, status, "pjsip_inv_send_msg() with 200");
                    free_leg (l);
                    return;
                }
                
            }
    
    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) 
    {
	    PJ_LOG(5,(FULL_INFO, "Call DISCONNECTED [reason=%d (%s)]", inv->cause, pjsip_get_status_text(inv->cause)->ptr));

	    PJ_LOG(5,(FULL_INFO, "One call completed, wait next one..."));

        free_junction (&g.junctions[l->junction_index]);
        return;

    } 
    else 
    {
	    PJ_LOG(5,(FULL_INFO, "\nCall state changed to %s\n", pjsip_inv_state_name(inv->state)));
    }    
}