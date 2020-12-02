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
    pjmedia_sock_info   media_sock_info;
    
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

    pjsip_endpt_respond_stateless(g.sip_endpt, rdata, 100, NULL, NULL, NULL);

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
    
    status = PJ_SUCCESS;
    junction_t *j=NULL;
    for (int i=0; i<10; i++)
    {
        if (g.junctions[i].state == READY)
            if ( status = pj_mutex_trylock(g.junctions[i].mutex) == PJ_SUCCESS   )
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
        pj_perror (5, "mutex", status, "hi");
        return PJ_FALSE;
    }


	
    /* Create UAS dialog */
    g.local_contact.ptr[g.local_contact.slen] = '\0';
    printf ("\n\n\nUAS:%s\n\n\n", g.local_contact.ptr);
    status = pjsip_dlg_create_uas_and_inc_lock( pjsip_ua_instance(), rdata,
						&g.local_contact, &dlg);

    if (status != PJ_SUCCESS) {
	reason = pj_str("Unable to create dialog");
	pjsip_endpt_respond_stateless( g.sip_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
    pj_perror (5, "dlg", status, "op");
	return PJ_TRUE;
    }

    /* Create SDP */
    //create_sdp( dlg->pool, &j->in_leg, &sdp);


	 


    /* Create UAS invite session */

    pjmedia_transport_info mti;
	pjmedia_transport_info_init(&mti);
	status = pjmedia_transport_get_info(j->in_leg.media_transport, &mti);

    pj_memcpy(&media_sock_info, &mti.sock_info,
		  sizeof(pjmedia_sock_info));
	
    pjsip_inv_session *inv;
    pjmedia_sdp_session *local_sdp;
	pjmedia_endpt_create_sdp(g.media_endpt, dlg->pool, 1, &media_sock_info, &local_sdp);

    j->out_leg.current.local_sdp = local_sdp;

    status = pjsip_inv_create_uas( dlg, rdata, local_sdp, 0, &inv);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_create_response(dlg, rdata, 500, NULL, &tdata);
	pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata), tdata);
	pjsip_dlg_dec_lock(dlg);
	return PJ_TRUE;
    }
    /*status = pjsip_inv_initial_answer(inv, rdata, 180, 
				      NULL, NULL, &tdata);
    pj_perror (5, "inv 180", status, "a");
	pjsip_inv_send_msg(inv, tdata); */ 
    
    // TODO: Here connect to ringback tonegen

    /* Invite session has been created, decrement & release dialog lock */
    pjsip_dlg_dec_lock(dlg);	

    /* Attach call data to invite session */ 
    inv->mod_data[g.mod_app.id] = &j->in_leg;

    /* Mark start of call */
    //pj_gettimeofday(&call->start_time);


	//pjsip_inv_answer(inv, 180, NULL, sdp, &tdata);
	 
    /* Create 183 response */
     
    j->in_leg.current.inv = inv;

    /* Send the 200 response. */

	// Here need sync both shoulders before
    if (make_call (tel, &j->out_leg) == PJ_TRUE)
    {
        pj_thread_create (g.pool, "junc_controller", junc_controller, j, 0, 0, &j->controller_thread);

        printf ("\n\n\n\nSEND 183\n\n\n");
        status = pjsip_inv_initial_answer(inv, rdata, 183, NULL, NULL, &tdata);
        if (status != PJ_SUCCESS) 
        {
	    pj_perror (5, "inv answer", status, "rx");
        status = pjsip_inv_answer(inv, PJSIP_SC_NOT_ACCEPTABLE, NULL, NULL, &tdata);
	    if (status == PJ_SUCCESS)
	        pjsip_inv_send_msg(inv, tdata); 
	    else
        {
            pjsip_inv_terminate(inv, 500, PJ_FALSE);
	    pj_perror (5, "inv answer", status, "rx");
        }
	        
	    return PJ_TRUE;
        }
        pjsip_inv_send_msg(inv, tdata);
    }
    else
    {
        pjsip_inv_terminate(inv, 500, PJ_FALSE);
        j->state = DISABLED;
    }
    

    // if failed then disable junc
    
    return PJ_TRUE;
   
}