#include "util.h"
extern struct global_var g;
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
    l->current.inv->mod_data[g.mod_app.id] = l;

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

pj_status_t create_sdp( pj_pool_t *pool,
			       leg_t *l,
			       pjmedia_sdp_session **p_sdp)
{
    pj_time_val tv;
    pjmedia_sdp_session *sdp;
    pjmedia_sdp_media *m;
    pjmedia_sdp_attr *attr;
    pjmedia_transport_info tpinfo;
    //struct media_stream *audio = &call->media[0];

    PJ_ASSERT_RETURN(pool && p_sdp, PJ_EINVAL);


    /* Get transport info */
    pjmedia_transport_info_init(&tpinfo);
    pjmedia_transport_get_info(l->media_transport, &tpinfo);

    /* Create and initialize basic SDP session */
    sdp = pj_pool_zalloc (pool, sizeof(pjmedia_sdp_session));

    pj_gettimeofday(&tv);
    sdp->origin.user = pj_str("pjsip-siprtp");
    sdp->origin.version = sdp->origin.id = tv.sec + 2208988800UL;
    sdp->origin.net_type = pj_str("IN");
    sdp->origin.addr_type = pj_str("IP4");
    sdp->origin.addr = *pj_gethostname();
    sdp->name = pj_str("pjsip");

    /* Since we only support one media stream at present, put the
     * SDP connection line in the session level.
     */
    sdp->conn = pj_pool_zalloc (pool, sizeof(pjmedia_sdp_conn));
    sdp->conn->net_type = pj_str("IN");
    sdp->conn->addr_type = pj_str("IP4");
    sdp->conn->addr = g.local_addr;


    /* SDP time and attributes. */
    sdp->time.start = sdp->time.stop = 0;
    sdp->attr_count = 0;

    /* Create media stream 0: */

    sdp->media_count = 1;
    m = pj_pool_zalloc (pool, sizeof(pjmedia_sdp_media));
    sdp->media[0] = m;

    /* Standard media info: */
    m->desc.media = pj_str("audio");
    m->desc.port = pj_ntohs(tpinfo.sock_info.rtp_addr_name.ipv4.sin_port);
    m->desc.port_count = 1;
    m->desc.transport = pj_str("RTP/AVP");

    /* Add format and rtpmap for each codec. */
    m->desc.fmt_count = 1;
    m->attr_count = 0;

    {
	pjmedia_sdp_rtpmap rtpmap;
	char ptstr[10];

	sprintf(ptstr, "%d", g.audio_codec.pt);
	pj_strdup2(pool, &m->desc.fmt[0], ptstr);
	rtpmap.pt = m->desc.fmt[0];
	rtpmap.clock_rate = g.audio_codec.clock_rate;
	rtpmap.enc_name = pj_str(g.audio_codec.name);
	rtpmap.param.slen = 0;

	pjmedia_sdp_rtpmap_to_attr(pool, &rtpmap, &attr);
	m->attr[m->attr_count++] = attr;
    }

    /* Add sendrecv attribute. */
    attr = pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    attr->name = pj_str("sendrecv");
    m->attr[m->attr_count++] = attr;


    /*
     * Add support telephony event
     */
    m->desc.fmt[m->desc.fmt_count++] = pj_str("121");
    /* Add rtpmap. */
    attr = pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    attr->name = pj_str("rtpmap");
    attr->value = pj_str("121 telephone-event/8000");
    m->attr[m->attr_count++] = attr;
    /* Add fmtp */
    attr = pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    attr->name = pj_str("fmtp");
    attr->value = pj_str("121 0-15");
    m->attr[m->attr_count++] = attr;


    /* Done */
    *p_sdp = sdp;

    return PJ_SUCCESS;
}
