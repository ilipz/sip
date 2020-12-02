#include "state_chd.h"

extern struct global_var g;


void on_state_changed (pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(e);
    const char THIS_FUNCTION[] = "call_on_state_changed()";

    
    
    leg_t *l = inv->mod_data[g.mod_app.id];
    if (l == NULL)
        return;

    if (l->type == OUT)
        if (l->current.inv)
            if (l->current.inv->state == PJSIP_INV_STATE_CONFIRMED)
            {
                pjsip_tx_data *tdata;
                pjsip_inv_answer(l->reverse->current.inv, 200, NULL, NULL, &tdata);
                pjsip_inv_send_msg(l->reverse->current.inv, tdata);
                printf ("\n\n\nCONFIRMED\n\n\n");
            }
    
    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) 
    {
	    PJ_LOG(3,(THIS_FUNCTION, "Call DISCONNECTED [reason=%d (%s)]", 
		  inv->cause,
		  pjsip_get_status_text(inv->cause)->ptr));

	    PJ_LOG(3,(THIS_FUNCTION, "One call completed, wait next one..."));

        free_junction (l->junction);
        return;

    } 
    else 
    {
	    PJ_LOG(3,(THIS_FUNCTION, "\nCall state changed to %s\n", 
		  pjsip_inv_state_name(inv->state)));
    }    
}