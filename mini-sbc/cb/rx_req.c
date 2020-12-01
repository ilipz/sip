#include "rx_req.h"

extern struct global_var g;
extern  numrecord_t nums[10];

pj_bool_t on_rx_request (pjsip_rx_data *rdata)
{
    unsigned i, options;
    printf ("\n\nPJ_FALSE\n\n");
    pjsip_dialog *dlg;
    pjmedia_sdp_session *sdp;
    pjsip_tx_data *tdata;
    pj_status_t status;
	pj_str_t reason;
    
    // Check validity

    /* Ignore strandled ACKs (must not send respone */
    
    if (rdata->msg_info.msg->line.req.method.id == PJSIP_ACK_METHOD)
	return PJ_FALSE;

    /* Respond (statelessly) any non-INVITE requests with 500  */
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {
	pj_str_t reason = pj_str("Unsupported Operation");
	pjsip_endpt_respond_stateless( g.sip_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
                       //printf ("\n\nPJ_FALSE\n\n");
	return PJ_TRUE;
    }


    /* Verify that we can handle the request. */
    options = 0;
    status = pjsip_inv_verify_request(rdata, &options, NULL, NULL,
  				   g.sip_endpt, &tdata);
    if (status != PJ_SUCCESS) {
	/*
	 * No we can't handle the incoming INVITE request.
	 */
	if (tdata) {
	    pjsip_response_addr res_addr;
	    
	    pjsip_get_response_addr(tdata->pool, rdata, &res_addr);
	    pjsip_endpt_send_response(g.sip_endpt, &res_addr, tdata,
		NULL, NULL);
	    
	} else {
	    
	    /* Respond with 500 (Internal Server Error) */
	    pjsip_endpt_respond_stateless(g.sip_endpt, rdata, 500, NULL,
		NULL, NULL);
	}
	
	return PJ_TRUE;
    }

    // Find in adddres book

    pjsip_sip_uri *sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(rdata->msg_info.to->uri);
	char telephone[64] =  {0};
    char *dest;
	strncpy (telephone, sip_uri->user.ptr, sip_uri->user.slen);

    numrecord_t *tel = get_numrecord (telephone);
	if (tel == NULL)
	{
		pjsip_endpt_respond_stateless(g.sip_endpt, rdata, 404, NULL, NULL, NULL);
		return PJ_TRUE;
	}

    


    // Find available junction

    junction_t *j=NULL;
    for (int i=0; i<10; i++)
    {
        if (g.junctions[i].state == READY)
            if ( PJ_SUCCESS == pj_mutex_trylock(g.junctions[i].mutex) )
            {
                j = &g.junctions[i];
                j->state = BUSY;
                pj_mutex_unlock (j->mutex);
                break;
            }
    }

    if (j == NULL)
    {
        PJ_LOG (5, ("on_rx_request", "Cann't find free junction"));
        return PJ_FALSE;
    }


	
    /* Create UAS dialog */
    status = pjsip_dlg_create_uas_and_inc_lock( pjsip_ua_instance(), rdata,
						&g.local_contact, &dlg);

    if (status != PJ_SUCCESS) {
	reason = pj_str("Unable to create dialog");
	pjsip_endpt_respond_stateless( g.sip_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
	return PJ_TRUE;
    }

    /* Create SDP */
    create_sdp( dlg->pool, &j->in_leg, &sdp);


	 


    /* Create UAS invite session */
	
    pjsip_inv_session *inv = j->in_leg.current.inv;

		

    status = pjsip_inv_create_uas( dlg, rdata, sdp, 0, &inv);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_create_response(dlg, rdata, 500, NULL, &tdata);
	pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata), tdata);
	pjsip_dlg_dec_lock(dlg);
	return PJ_TRUE;
    }
    pjsip_inv_initial_answer(inv, rdata, 180, 
				      NULL, sdp, &tdata);
	pjsip_inv_send_msg(inv, tdata); 
    
    // TODO: Here connect to ringback tonegen

    /* Invite session has been created, decrement & release dialog lock */
    pjsip_dlg_dec_lock(dlg);	

    /* Attach call data to invite session */ 
    inv->mod_data[g.mod_app.id] = &j->in_leg;

    /* Mark start of call */
    //pj_gettimeofday(&call->start_time);


	//pjsip_inv_answer(inv, 180, NULL, sdp, &tdata);
	pjsip_inv_send_msg(inv, tdata); 
    /* Create 183 response .*/
    status = pjsip_inv_answer(inv, 183, NULL, NULL, &tdata);
    if (status != PJ_SUCCESS) 
    {
	    status = pjsip_inv_answer(inv, PJSIP_SC_NOT_ACCEPTABLE, NULL, NULL, &tdata);
	    if (status == PJ_SUCCESS)
	        pjsip_inv_send_msg(inv, tdata); 
	    else
	        pjsip_inv_terminate(inv, 500, PJ_FALSE);
	return PJ_TRUE;
    }


    /* Send the 200 response. */

	// Here need sync both shoulders before
    if (make_call (tel, &j->out_leg) == PJ_TRUE)
    {
        pj_thread_create (g.pool, "junc_controller", junc_controller, j, 0, 0, &j->controller_thread);
    }
    else
    {
        pjsip_inv_terminate(inv, 500, PJ_FALSE);
        j->state = DISABLED;
    }
    

    // if failed then disable junc
    
    return PJ_TRUE;
   
}