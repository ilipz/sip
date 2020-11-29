// *copyrights*
/* NOTES
    + do not use media event mgr
    + use inv_end_session for free_* functions
*/
#include <pjsip.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <pthread.h>
#include <stdlib.h>

#define PJ_LOG_ERROR    "!!! ERROR: " 
#define APPNAME         "MINI-SBC: "

/////// TYPES

struct codec
{
    unsigned	pt;
    char*	name;
    unsigned	clock_rate;
    unsigned	bit_rate;
    unsigned	ptime;
    char*	description;
};

typedef struct leg
{
    struct current_t
        {
            pjsip_inv_session   *inv;
            pjmedia_port        *stream_port;
            pjmedia_stream_info *stream_info;
            pjmedia_stream      *stream;

            pjmedia_rtp_session out_session;
            pjmedia_rtp_session in_session;
            pjmedia_rtcp_session rtcp_session;
            pjmedia_sdp_session *local_sdp;
            pjmedia_sdp_session *remote_sdp;
        } current;
    
    pjmedia_transport   *media_transport;
    leg_t          *reverse;    
    enum leg_type {IN=0, OUT=1} type;
} leg_t;

typedef struct junction
{
    leg_t  in_leg;
    leg_t  out_leg;
    pjmedia_master_port *mp;
    enum state_t {BUSY=0, READY, DISABLED} state;
    pj_mutex_t  *mutex;
    int index; 
} junction_t;

typedef struct numrecord
{
	char num[8];
	char addr[32];
} numrecord_t;

struct global_var
{
    pjsip_endpoint  *sip_endpt;
    pjmedia_endpt   *media_endpt;

    pj_caching_pool cp;
    pj_pool_t       *pool;


    pj_str_t		 local_uri;
    pj_str_t		 local_contact;
    pj_str_t		 local_addr;

    pj_uint16_t     sip_port;
    pj_uint16_t     rtp_port;

    junction_t      junctions[10];
    numrecord_t     numbook[20];

    pjmedia_conf    *tonegen_conf;
    pjmedia_port    *tonegen_port;
    unsigned        tonegen_port_id;

    pj_bool_t       pause;

    pjmedia_conf    *conf;
    pjmedia_master_port *conf_mp;
    pjmedia_port    *conf_null_port;
    FILE *log_file;
    unsigned rtp_start_port;

    struct codec audio_codec;

};


/////// FUNCTIONS

int console_thread (void *p);
void emergency_exit (char *sender, pj_status_t *status); // use NULL instead of status if not need
void halt ();
void free_junction (junction_t *j);
void free_leg (leg_t *l);
void nullize_leg (leg_t *l);
void destroy_junction (junction_t *j);
char num_addr (char *num);

void on_media_update (pjsip_inv_session *inv, pjsip_status_t status);
void on_state_changed ( pjsip_inv_session *inv, pjsip_event *e);
void on_forked (pjsip_inv_session *inv, pjsip_event *e);
pj_bool_t on_rx_request (pjsip_rx_data *rdata);
pj_bool_t logger_on_rx_msg(pjsip_rx_data *rdata);
pj_status_t logger_on_tx_msg(pjsip_tx_data *tdata);

pj_status_t create_sdp (pj_pool_t *pool, leg_t *l, pjmedia_sdp_session **sdp);
void init_sip ();
//void destroy_sip ();
void init_media ();
//void destroy_media ();
void make_call (numrecord_t *tel, leg_t *l);

/////// MODULES

pjsip_module mod_app =
{
    NULL, NULL,			    /* prev, next.		*/
    { "App module", 13 },	    /* Name.			*/
    -1,				    /* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION, /* Priority			*/
    NULL,			    /* load()			*/
    NULL,			    /* start()			*/
    NULL,			    /* stop()			*/
    NULL,			    /* unload()			*/
    &on_rx_request,		    /* on_rx_request()		*/
    NULL,			    /* on_rx_response()		*/  
    NULL,			    /* on_tx_request.		*/ //here may be ACK is sent
    NULL,			    /* on_tx_response()		*/
    NULL,			    /* on_tsx_state()		*/
};

pjsip_module mod_logger = 
{
    NULL, NULL,				/* prev, next.		*/
    { "Logger module", 14 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &logger_on_rx_msg,			/* on_rx_request()	*/
    &logger_on_rx_msg,			/* on_rx_response()	*/
    &logger_on_tx_msg,			/* on_tx_request.	*/
    &logger_on_tx_msg,			/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};

/////// VARS

struct codec audio_codecs[] = 
{
    { 0,  "PCMU", 8000, 16000, 20, "G.711 ULaw" },
    { 3,  "GSM",  8000, 13200, 20, "GSM" },
    { 4,  "G723", 8000, 6400,  30, "G.723.1" },
    { 8,  "PCMA", 8000, 16000, 20, "G.711 ALaw" },
    { 18, "G729", 8000, 8000,  20, "G.729" },
};

struct global_var g;

numrecord_t nums[10] = 
{
    {"05", "127.0.0.1:5060"},
    {"444", "127.0.0.1:5060"},
    {"9000", "127.0.0.1:5060"},
    {"1234", "127.0.0.1:5060"}
};

/////// REFERENCES

pj_status_t init_media()
{
    //unsigned	i, count;
    pj_uint16_t	rtp_port;
    pj_status_t	status;
    unsigned count=0;
    g.rtp_start_port = 4000;

    /* Initialize media endpoint so that at least error subsystem is properly
     * initialized.
     */

    status = pjmedia_endpt_create(&g.cp.factory, NULL, 1, &g.media_endpt);

    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Must register codecs to be supported */

    pjmedia_codec_g711_init(g.media_endpt);


    /* RTP port counter */
    rtp_port = (pj_uint16_t)(g.rtp_start_port & 0xFFFE);

    for (int current_junc=0; current_junc<10; current_junc++)
    {
        junction_t *j = &g.junctions[current_junc];

        if (j->state == DISABLED)
            continue;
        count++;
        for (int retry=0; retry<=1500; retry++)
        {
            status = pjmedia_transport_udp_create2 (g.media_endpt, "in_leg", &g.local_addr, rtp_port++, 0, &j->in_leg.media_transport);
            if (PJ_SUCCESS != status)
            {
                pj_perror (5, "init media()", status, "pjmedia_transport_udp_create2");
                j->state = DISABLED;
                count--;
                break;
            }
        }

        if (j->state == DISABLED)
            break;
        for (int retry=0; retry<=1500; retry++)
        {
            status = pjmedia_transport_udp_create2 (g.media_endpt, "in_leg", &g.local_addr, rtp_port++, 0, &j->out_leg.media_transport);
            if (PJ_SUCCESS != status)
            {
                pj_perror (5, "init media()", status, "pjmedia_transport_udp_create2");
                j->state = DISABLED;
                count--;
                pjmedia_transport_close (j->in_leg.media_transport);
                break;
            }

        }
        
    }

    if (count == 0); // emergency exit no one transport inited

}

void init_sip()
{
    unsigned i;
    pj_status_t status;

    
    /* 
    init in main:
    
    status = pjlib_util_init(); 
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    
    pj_caching_pool_init(&app.cp, &pj_pool_factory_default_policy, 0);

    
    app.pool = pj_pool_create(&app.cp.factory, "app", 1000, 1000, NULL);

    */

    /* Create the endpoint: */
    status = pjsip_endpt_create(&g.cp.factory, pj_gethostname()->ptr, 
				&g.sip_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Add UDP transport. */
    {
	pj_sockaddr_in addr;
	pjsip_host_port addrname;
	pjsip_transport *tp;

	pj_bzero(&addr, sizeof(addr));
	addr.sin_family = pj_AF_INET();
	addr.sin_addr.s_addr = 0;
	addr.sin_port = pj_htons((pj_uint16_t)g.sip_port); 

	if (app.local_addr.slen) {

	    addrname.host = g.local_addr;
	    addrname.port = g.sip_port;

	    status = pj_sockaddr_in_init(&addr, &g.local_addr, 
					 (pj_uint16_t)g.sip_port);
	    
	}

	status = pjsip_udp_transport_start( g.sip_endpt, &addr, 
					    (g.local_addr.slen ? &addrname:NULL),
					    2, &tp);
	

	PJ_LOG(3,(THIS_FILE, "SIP UDP listening on %.*s:%d",
		  (int)tp->local_name.host.slen, tp->local_name.host.ptr,
		  tp->local_name.port));
    }

    /* 
     * Init transaction layer.
     * This will create/initialize transaction hash tables etc.
     */
    status = pjsip_tsx_layer_init_module(g.sip_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /*  Initialize UA layer. */
    status = pjsip_ua_init_module( g.sip_endpt, NULL );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Initialize 100rel support */
    status = pjsip_100rel_init_module(g.sip_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /*  Init invite session module. */
    {
	pjsip_inv_callback inv_cb;

	/* Init the callback for INVITE session: */
	pj_bzero(&inv_cb, sizeof(inv_cb));
	inv_cb.on_state_changed = &call_on_state_changed;
	inv_cb.on_new_session = &call_on_forked;
	inv_cb.on_media_update = &call_on_media_update;
	inv_cb.on_send_ack = &on_send_ack;

	/* Initialize invite session module:  */
	status = pjsip_inv_usage_init(g.sip_endpt, &inv_cb);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }

    /* Register our module to receive incoming requests. */
    status = pjsip_endpt_register_module( g.sip_endpt, &mod_siprtp);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Done */
    
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
				   &app.local_uri,	/* local URI	    */
				   &app.local_contact,	/* local Contact    */
				   &dst_uri,		/* remote URI	    */
				   &dst_uri,		/* remote target    */
				   &dlg);		/* dialog	    */
    if (status != PJ_SUCCESS) 
        return PJ_FALSE;

    /* Create SDP */
    create_sdp( dlg->pool, call, &sdp);

    /* Create the INVITE session. */
    status = pjsip_inv_create_uac( dlg, sdp, 0, &l->current.inv);
    if (status != PJ_SUCCESS) 
    {
	    pjsip_dlg_terminate(dlg);
        return PJ_FALSE;
    }


    /* Attach call data to invite session */
    l->current.inv->mod_data[mod_siprtp.id] = l;

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

pj_bool_t on_rx_request (pjsip_rx_data *rdata)
{
    /* Ignore strandled ACKs (must not send respone */
    if (rdata->msg_info.msg->line.req.method.id == PJSIP_ACK_METHOD)
	return PJ_FALSE;

    /* Respond (statelessly) any non-INVITE requests with 500  */
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {
	pj_str_t reason = pj_str("Unsupported Operation");
	pjsip_endpt_respond_stateless( g.sip_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
	return PJ_TRUE;
    }

    unsigned i, options;
    struct call *call;
    pjsip_dialog *dlg;
    pjmedia_sdp_session *sdp;
    pjsip_tx_data *tdata;
    pj_status_t status;
	pj_str_t reason;



    junction_t *j=NULL;


    // Find unused junction

    for (int i=0; i<10; i++)
    {
        if (g.junctions[i].state == READY)
            if ( PJ_SUCCESS == pj_mutex_trylock(g.junctions[i].mutex) )
            {
                j = &g.junctions[i];
                j->state = BUSY;
                pj_mutex_unlock (j->mutex);
            }
    }

    if (j == NULL)
    {
        PJ_LOG (5, ("on_rx_request", "Cann't find free junction"));
        return PJ_FALSE;
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
	
	return;
    }

	pjsip_sip_uri *sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(rdata->msg_info.to->uri);
	char telephone[64] =  {0};
    char *dest;
	strncpy (telephone, sip_uri->user.ptr, sip_uri->user.slen);

    numrecord_t *tel;
    num_addr (tel);
	if (tel == NULL)
	{
		pjsip_endpt_respond_stateless(g.sip_endpt, rdata, 404, NULL, NULL, NULL);
		return;
	}

	

	

	pjsip_inv_initial_answer(inv, rdata, 100, 
				      NULL, NULL, &tdata);
	pjsip_inv_send_msg(inv, tdata); 
    /* Create UAS dialog */
    status = pjsip_dlg_create_uas_and_inc_lock( pjsip_ua_instance(), rdata,
						&g.local_contact, &dlg);

    if (status != PJ_SUCCESS) {
	reason = pj_str("Unable to create dialog");
	pjsip_endpt_respond_stateless( g.sip_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
	return;
    }

    /* Create SDP */
    create_sdp( dlg->pool, &sdp);


	 


    /* Create UAS invite session */
	
    pjsip_inv_session *inv = j->in_leg.current.inv;

		

    status = pjsip_inv_create_uas( dlg, rdata, sdp, 0, &inv);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_create_response(dlg, rdata, 500, NULL, &tdata);
	pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata), tdata);
	pjsip_dlg_dec_lock(dlg);
	return;
    }
    
    /* Invite session has been created, decrement & release dialog lock */
    pjsip_dlg_dec_lock(dlg);	

    /* Attach call data to invite session */
    inv->mod_data[mod_app.id] = j->in_leg;

    /* Mark start of call */
    pj_gettimeofday(&call->start_time);


	pjsip_inv_answer(inv, rdata, 180, 
				      NULL, NULL, &tdata);
	pjsip_inv_send_msg(inv, tdata); 
    /* Create 183 response .*/
    status = pjsip_inv_answer(inv, rdata, 183, 
				      NULL, NULL, &tdata);
    if (status != PJ_SUCCESS) {
	status = pjsip_inv_answer(inv, rdata, 
					  PJSIP_SC_NOT_ACCEPTABLE,
					  NULL, NULL, &tdata);
	if (status == PJ_SUCCESS)
	    pjsip_inv_send_msg(inv, tdata); 
	else
	    pjsip_inv_terminate(inv, 500, PJ_FALSE);
	return;
    }


    /* Send the 200 response. */

	// Here need sync both shoulders before
    make_call (tel, &j->out_leg);
    // if failed then disable junc
    // start connector thread
   
   
}

pj_status_t create_sdp( pj_pool_t *pool,
			       junction_t *j,
			       pjmedia_sdp_session **p_sdp)
{
    pj_time_val tv;
    pjmedia_sdp_session *sdp;
    pjmedia_sdp_media *m;
    pjmedia_sdp_attr *attr;
    pjmedia_transport_info tpinfo;
    

    PJ_ASSERT_RETURN(pool && p_sdp, PJ_EINVAL);


    /* Get transport info */
    pjmedia_transport_info_init(&tpinfo);
    pjmedia_transport_get_info(j->out_leg.media_transport, &tpinfo);

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
    sdp->conn->addr = app.local_addr;


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