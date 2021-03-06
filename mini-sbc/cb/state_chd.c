// Sending 200 OK temporarly disabled 

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
    char *type = l->type==IN ? "IN" : "OUT";
    sprintf (FULL_INFO, "%s with %s leg in junc#%u", THIS_FUNCTION, l->type == IN ? "IN" : "OUT", l->junction_index);

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) 
    {
	    PJ_LOG(5,(FULL_INFO, "Call DISCONNECTED [reason=%d (%s)]", inv->cause, pjsip_get_status_text(inv->cause)->ptr));

	    PJ_LOG(5,(FULL_INFO, "Disconnect junc..."));

        free_junction (&g.junctions[l->junction_index]);
        return;

    } 
    else 
    {
	    PJ_LOG(5,(FULL_INFO, "\nCall state changed to %s (%s leg)\n", pjsip_inv_state_name(inv->state), type));
    }    
}