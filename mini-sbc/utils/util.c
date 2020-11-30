#include "util.h"

numrecord_t *get_numrecord (char *num)
{
	for (int i=0; i<10; i++)
		if (!strcmp(nums[i].num, num))
			return &nums[i];
	return NULL;
}

pj_bool_t make_call(numrecord_t *tel, leg_t *l)//, pjmedia_sdp_session *sdp)
{
    // TODO: here if any function fail then make junction disabled
    char dest_uri;
    sprintf (dest_uri, "sip:%s@%s", tel->num, tel->addr);
    pj_str_t dst_uri = pj_str (dest_uri);
    unsigned i;
    pjsip_dialog *dlg;
	pjmedia_sdp_session *sdp;
    pjsip_tx_data *tdata;
    pj_status_t status;
    

	
    /* Create UAC dialog */
    status = pjsip_dlg_create_uac( pjsip_ua_instance(), 
				   &g.local_uri,	/* local URI	    */
				   &g.local_contact,	/* local Contact    */
				   &dst_uri,		/* remote URI	    */
				   &dst_uri,		/* remote target    */
				   &dlg);		/* dialog	    */
    if (status != PJ_SUCCESS) 
        return PJ_FALSE;

    /* Create SDP */
    create_sdp( dlg->pool, l, &sdp);

    /* Create the INVITE session. */
    status = pjsip_inv_create_uac( dlg, sdp, 0, &l->current.inv);
    if (status != PJ_SUCCESS) 
    {
	    pjsip_dlg_terminate(dlg);
        return PJ_FALSE;
    }


    /* Attach call data to invite session */
    l->current.inv->mod_data[mod_app.id] = l;

    /* Mark start of call */
   // pj_gettimeofday(&call->start_time);
    //if (status != PJ_SUCCESS) 
     //   return PJ_FALSE;


    /* Create initial INVITE request.
     * This INVITE request will contain a perfectly good request and 
     * an SDP body as well.
     */
    status = pjsip_inv_invite(l->current.inv, &tdata);
    if (status != PJ_SUCCESS) 
        return PJ_FALSE;


    /* Send initial INVITE request. 
     * From now on, the invite session's state will be reported to us
     * via the invite session callbacks.
     */
    status = pjsip_inv_send_msg(l->current.inv, tdata);
    if (status != PJ_SUCCESS) 
        return PJ_FALSE;


    return PJ_TRUE;
}


