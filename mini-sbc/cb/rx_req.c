// Catching errors temporarly switched to fast-fail
#include "rx_req.h"

extern struct global_var g;
extern  numrecord_t nums[10];

static pjsip_rx_data *rdata;
static char *THIS_FUNCTION = "on_rx_request()";
static char FULL_INFO[64] = "on_rx_request()";
static unsigned long long req_id;

static pj_bool_t send_internal_error (char *reason)
{
    pj_status_t status;
    status = pjsip_endpt_respond_stateless(g.sip_endpt, rdata, 500, NULL, NULL, NULL);
    if (status != PJ_SUCCESS)
    {
        pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjsip_endpt_respond_stateless() for request #%llu due to %s crash", req_id, reason);
        return PJ_FALSE;
    }
    PJ_LOG (5, (FULL_INFO, "Sent 500 Internal Error due to %s crash", reason));
    return PJ_TRUE;
}

pj_bool_t on_rx_request (pjsip_rx_data *r)
{
    rdata = r;
    
    if (rdata == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"Gotten empty rdata. Returned PJ_FALSE"));
        return PJ_FALSE;
    }

    req_id = g.req_id++;

    PJ_LOG (5, (THIS_FUNCTION, "Called for #%llu request", req_id));
    
    unsigned i, options;
    pjsip_dialog *dlg;
    pjmedia_sdp_session *sdp;
    pjsip_tx_data *tdata;
    pj_status_t status;
	pj_str_t reason;
    pjmedia_sock_info   media_sock_info;
    
    
    
    
    if (rdata->msg_info.msg->line.req.method.id == PJSIP_ACK_METHOD)
    {
        PJ_LOG (5, (THIS_FUNCTION, "ACK method ignored (request #%llu)", req_id));
        return PJ_FALSE;
    }
	    

    /* Respond (statelessly) any non-INVITE requests with 500  */
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) 
    {
	    pj_str_t reason = pj_str("Unsupported Operation");
	    PJ_LOG (5, (THIS_FUNCTION, "Unsupported #%llu request gotten", req_id));

        status = pjsip_endpt_respond_stateless( g.sip_endpt, rdata, 500, &reason, NULL, NULL);
        if (status != PJ_SUCCESS)
        {
            pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pjsip_endpt_respond_stateless() with 500 Internal Error for non-INVITE request");
        }
        else
        {
            PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"Sent 500 due to unsupported request #%llu", req_id));
        }
        
	    return PJ_TRUE;
    }

    /*status = pjsip_endpt_respond_stateless(g.sip_endpt, rdata, 100, NULL, NULL, NULL);
    if (status != PJ_SUCCESS)
    {
        pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pjsip_endpt_respond_stateless() with 100 TRYING for request #%llu", req_id);
        return PJ_TRUE;
    } */

    // Verify that we can handle the request. 
    options = 0;
    status = pjsip_inv_verify_request(rdata, &options, NULL, NULL, g.sip_endpt, &tdata);
    if (status != PJ_SUCCESS) 
    {
	    pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pjsip_inv_verify_request() for request #%llu", req_id);
	    
	    if (tdata) 
        {
	        pjsip_response_addr res_addr; 
	        
            status = pjsip_get_response_addr(tdata->pool, rdata, &res_addr);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pjsip_get_response_addr() for request #%llu", req_id);
            }
            else
            {
                status = pjsip_endpt_send_response (g.sip_endpt, &res_addr, tdata, NULL, NULL);
                if (status != PJ_SUCCESS)
                {
                    pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pjsip_endpt_send_response() for request #%llu", req_id);
                }
            }
	    } 
        else 
        {
	        
	    }
	
	    return PJ_TRUE;
    }

    // Find in adddres book

    pjsip_sip_uri *sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(rdata->msg_info.to->uri);
    if (sip_uri == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"Cann't get sip_uri for request #%llu", req_id));
        return PJ_TRUE;
    }

	char telephone[64] =  {0};
    char *dest;
	strncpy (telephone, sip_uri->user.ptr, sip_uri->user.slen);

    numrecord_t *tel = get_numrecord (telephone);
	if (tel == NULL)
	{
		PJ_LOG (5, (THIS_FUNCTION, "Number %s not found (req #%llu). Session aborted", telephone, req_id));
        status = pjsip_endpt_respond_stateless (g.sip_endpt, rdata, 404, NULL, NULL, NULL);
        if (sip_uri == NULL)
        {
            pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pjsip_endpt_respond_stateless() with 404 for request #%llu", req_id);
        }
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
                status = pj_mutex_unlock (j->mutex);
                if (status != PJ_SUCCESS)
                {
                    pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pj_mutex_unlock() in junc#%u (req #%llu)", j->index, req_id);
                    emergency_exit ("on_rx_request()::pj_mutex_unlock()", NULL);
                }
                PJ_LOG (5, (THIS_FUNCTION, "Req #%llu assigned with junc#%u", req_id, j->index));
                break;
            }
    }

    if (j == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, "Cann't find free junction for req #%llu", req_id));
        status = pjsip_endpt_respond_stateless (g.sip_endpt, rdata, 486, NULL, NULL, NULL);
        if (status != PJ_SUCCESS)
            halt ("rx_req.c");
        if (sip_uri == NULL)
        {
            pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pjsip_endpt_respond_stateless() with 486 for request #%llu", req_id);
        }
        return PJ_TRUE;
    }

    sprintf (FULL_INFO, "Req %llu in junc#%u (IN leg)", req_id, j->index);
	PJ_LOG (5, (FULL_INFO, "Connecting to %s with telnum %s", tel->addr, tel->num));
    /* Create UAS dialog */
    PJ_LOG (5, (FULL_INFO, "Creating UAS dialog..."));
    g.local_contact.ptr[g.local_contact.slen] = '\0';
    status = pjsip_dlg_create_uas_and_inc_lock( pjsip_ua_instance(), rdata, &g.local_contact, &dlg);
    if (status != PJ_SUCCESS)
            halt ("rx_req.c");
    /*if (status != PJ_SUCCESS) 
    {
	    pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjsip_dlg_create_uas_and_inc_lock()");
        
        reason = pj_str("Unable to create dialog");
	    status = pjsip_endpt_respond_stateless( g.sip_endpt, rdata, 500, &reason, NULL, NULL);
        if (status != PJ_SUCCESS)
        {
            pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjsip_endpt_respond_stateless() with 500 in creating UAS dialog");
        }
	    return PJ_TRUE;
    }*/

    /* Create UAS invite session */

    pjmedia_transport_info mti;
	pjmedia_transport_info_init(&mti);
	status = pjmedia_transport_get_info(j->in_leg.media_transport, &mti);
    if (status != PJ_SUCCESS)
            halt ("rx_req.c");
    /*if (status != PJ_SUCCESS)
    {
        pj_perror (5, FULL_INFO, status, "pjmedia_transport_get_info()");
        send_internal_error ("pjmedia_transport_get_info()");
        return PJ_TRUE;
    }*/

    pj_memcpy(&media_sock_info, &mti.sock_info, sizeof(pjmedia_sock_info));
	
    pjsip_inv_session *inv;
    pjmedia_sdp_session *local_sdp;
	status = pjmedia_endpt_create_sdp(g.media_endpt, dlg->pool, 1, &media_sock_info, &local_sdp);
    if (status != PJ_SUCCESS)
            halt ("rx_req.c");
    /*if (status != PJ_SUCCESS)
    {
        pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjmedia_endpt_create_sdp()");
        send_internal_error ("pjmedia_endpt_create_sdp()");
        return PJ_TRUE;
    }*/

    

    status = pjsip_inv_create_uas (dlg, rdata, local_sdp, 0, &inv);
    if (status != PJ_SUCCESS)
            halt ("rx_req.c");
    /*if (status != PJ_SUCCESS) 
    {
	    status = pjsip_dlg_create_response(dlg, rdata, 500, NULL, &tdata);
        if (status != PJ_SUCCESS)
        {
            pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjsip_dlg_create_response()");
            //send_internal_error ("pjsip_dlg_create_response()");
        }

	    status = pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata), tdata);
	    if (status != PJ_SUCCESS)
        {
            pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjsip_dlg_create_response()");
            //send_internal_error ("pjsip_dlg_create_response()");
        }
        
        pjsip_dlg_dec_lock(dlg);
	return PJ_TRUE;
    } */
    status = pjsip_inv_initial_answer(inv, rdata, 100, NULL, NULL, &tdata);
    if (status != PJ_SUCCESS)
            halt ("rx_req.c");
    
    status = pjsip_inv_send_msg(inv, tdata);
    if (status != PJ_SUCCESS)
            halt ("rx_req.c");
    /*
        if (status != PJ_SUCCESS)
        {
            pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjsip_inv_send_msg() with 100");
            send_internal_error ("pjsip_inv_send_msg() with 100");
            return PJ_TRUE;
        } */

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
    status = pjsip_inv_answer(inv,  180, NULL, NULL, &tdata);
    if (status != PJ_SUCCESS)
            halt ("rx_req.c");
    
    status = pjsip_inv_send_msg(inv, tdata);
     if (status != PJ_SUCCESS)
            halt ("rx_req.c");

    if (make_call (tel, &j->out_leg) == PJ_TRUE)
    {
        status = pj_thread_create (g.pool, "junc_controller", junc_controller, j, 0, 0, &j->controller_thread);
        if (status != PJ_SUCCESS)
            halt ("rx_req.c");
    }
    else
    {
        halt ("rx_req.c");
        return PJ_TRUE;
        status = pjsip_endpt_respond_stateless(g.sip_endpt, rdata, 503, NULL, NULL, NULL);
        if (status != PJ_SUCCESS)
        {
            pj_perror (5, FULL_INFO, status, PJ_LOG_ERROR"pjsip_endpt_respond_stateless() with 503");
            return PJ_TRUE;
        }
        PJ_LOG (5, (FULL_INFO, "Sent 503 Internal Error due to OUT leg call failed", reason));
    }

    
    return PJ_TRUE;
   
}