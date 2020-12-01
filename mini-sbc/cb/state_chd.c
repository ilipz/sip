#include "state_chd.h"

extern struct global_var g;


void on_state_changed (pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(e);
    const char THIS_FUNCTION[] = "call_on_state_changed()";

    /*int index = get_slot_by_inv (inv);
    if (index == -1)
    {
        PJ_LOG (5, (THIS_FUNCTION, "error getting slot index. Function halted"));
        return;
    }
    
    slot_t *slot = &slots[index]; // through inv->mod_data
    if (!slot->busy)
        return;*/
    
    leg_t *l = inv->mod_data[g.mod_app.id];
    if (l == NULL)
        return;

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