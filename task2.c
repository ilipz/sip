#include <pjsip.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjlib-util.h>
#include <pjlib.h>

void poolfail_cb (pj_pool_t *pool, pj_size_t size)
{
    pj_perror (5, "app", 0, "Ploho" );
    exit (1);
}


// For logging purpose. 
#define THIS_FILE   "simpleua.c"

// Settings 
#define AF		pj_AF_INET() // Change to pj_AF_INET6() for IPv6.
//PJ_HAS_IPV6 must be enabled and
				      // your system must support IPv6.  

#define SIP_PORT	5060	     // Listening SIP port		
#define RTP_PORT	4000	     // RTP port			

#define MAX_MEDIA_CNT	1	     // Media count, set to 1 for audio
				      //only or 2 for audio and video	


// Static variables.
 

static pj_bool_t	     g_complete;    // Quit flag.		
static pjsip_endpoint	    *g_endpt;	    // SIP endpoint.		
static pj_caching_pool	     cp;	    // Global pool factory.	
pj_pool_t *pool = NULL;

pjsip_dialog *cdlg;

static pjmedia_endpt	    *g_med_endpt;   // Media endpoint.		

static pjmedia_transport_info g_med_tpinfo[MAX_MEDIA_CNT]; 
					    // Socket info for media	
static pjmedia_transport    *g_med_transport[MAX_MEDIA_CNT];
					    // Media stream transport	
static pjmedia_sock_info     g_sock_info[MAX_MEDIA_CNT];  
					    // Socket info array	

// Call variables: 
static pjsip_inv_session    *g_inv;	    // Current invite session.	
static pjmedia_stream       *g_med_stream;  // Call's audio stream.	
static pjmedia_snd_port	    *sound_port;    // Sound device.		


// Junction between tonegen/file port and stream port through pj_master_port

pjmedia_master_port *mp=NULL;
pjmedia_port *tonegen_port=NULL;
pjmedia_port *stream_port=NULL;
pjmedia_port *player_port=NULL;
pjmedia_port *out_port=NULL;
pjsip_rx_data *r2data=NULL;

pjsip_tx_data *tdata=NULL;


pj_timer_entry entry[2];
pj_timer_heap_t *timer;
pj_time_val delay;
static pjsip_inv_session *g2_inv; // for timer's callback	



void pjtimer_callback(pj_timer_heap_t *ht, pj_timer_entry *e)
{
    pj_status_t status;
    PJ_UNUSED_ARG(ht);
    PJ_UNUSED_ARG(e);

    
    status =   pjsip_inv_answer
                (
                    g_inv, 200, NULL, NULL, &tdata 
                );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

    
    status = pjsip_inv_send_msg(g_inv, tdata);
   

}

void auto_exit (pj_timer_heap_t *ht, pj_timer_entry *e)
{
    PJ_UNUSED_ARG(ht);
    PJ_UNUSED_ARG(e);
    g_complete = 1;
    pjsip_tx_data *bye_data;
    pjsip_dlg_create_request (cdlg, &pjsip_bye_method, -1, &bye_data);
    pjsip_dlg_send_request (cdlg, bye_data, -1, NULL); 
}

// Callback to be called when SDP negotiation is done in the call: 
static void call_on_media_update( pjsip_inv_session *inv,
				  pj_status_t status);

// Callback to be called when invite session's state has changed: 
static void call_on_state_changed( pjsip_inv_session *inv, 
				   pjsip_event *e);

// Callback to be called when dialog has forked: */
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e);

// Callback to be called to handle incoming requests outside dialogs: 
static pj_bool_t on_rx_request( pjsip_rx_data *rdata );




/* This is a PJSIP module to be registered by application to handle
 * incoming requests outside any dialogs/transactions. The main purpose
 * here is to handle incoming INVITE request message, where we will
 * create a dialog and INVITE session for it.
 */
static pjsip_module mod_simpleua =
{
    NULL, NULL,			    // prev, next.		
    { "mod-simpleua", 12 },	    // Name.			
    -1,				    // Id			
    PJSIP_MOD_PRIORITY_APPLICATION, // Priority			
    NULL,			    // load()			
    NULL,			    // start()			
    NULL,			    // stop()			
    NULL,			    // unload()			
    &on_rx_request,		    // on_rx_request()		
    NULL,			    // on_rx_response()		
    NULL,			    // on_tx_request.		
    NULL,			    // on_tx_response()		
    NULL,			    // on_tsx_state()		
};



// Notification on incoming messages 
static pj_bool_t logging_on_rx_msg(pjsip_rx_data *rdata)
{
    PJ_LOG(4,(THIS_FILE, "RX %d bytes %s from %s %s:%d:\n"
			 "%.*s\n"
			 "--end msg--",
			 rdata->msg_info.len,
			 pjsip_rx_data_get_info(rdata),
			 rdata->tp_info.transport->type_name,
			 rdata->pkt_info.src_name,
			 rdata->pkt_info.src_port,
			 (int)rdata->msg_info.len,
			 rdata->msg_info.msg_buf));
    
    // Always return false, otherwise messages will not get processed! 
    return PJ_FALSE;
}

// Notification on outgoing messages 
static pj_status_t logging_on_tx_msg(pjsip_tx_data *tdata)
{
    
    /* Important note:
     *	tp_info field is only valid after outgoing messages has passed
     *	transport layer. So don't try to access tp_info when the module
     *	has lower priority than transport layer.
     */

    PJ_LOG(4,(THIS_FILE, "TX %d bytes %s to %s %s:%d:\n"
			 "%.*s\n"
			 "--end msg--",
			 (tdata->buf.cur - tdata->buf.start),
			 pjsip_tx_data_get_info(tdata),
			 tdata->tp_info.transport->type_name,
			 tdata->tp_info.dst_name,
			 tdata->tp_info.dst_port,
			 (int)(tdata->buf.cur - tdata->buf.start),
			 tdata->buf.start));

    // Always return success, otherwise message will not get sent! 
    return PJ_SUCCESS;
}

// The module instance. 
static pjsip_module msg_logger = 
{
    NULL, NULL,				// prev, next.		
    { "mod-msg-log", 13 },		// Name.		
    -1,					// Id			
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,// Priority	        
    NULL,				// load()		
    NULL,				// start()		
    NULL,				// stop()		
    NULL,				// unload()		
    &logging_on_rx_msg,			// on_rx_request()	
    &logging_on_rx_msg,			// on_rx_response()	
    &logging_on_tx_msg,			// on_tx_request.	
    &logging_on_tx_msg,			// on_tx_response()	
    NULL,				// on_tsx_state()	

};


/*
 * Callback when INVITE session state has changed.
 * This callback is registered when the invite session module is initialized.
 * We mostly want to know when the invite session has been disconnected,
 * so that we can quit the application.
 */

static void call_on_state_changed( pjsip_inv_session *inv, 
				   pjsip_event *e)
{
    PJ_UNUSED_ARG(e);

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {

	PJ_LOG(3,(THIS_FILE, "Call DISCONNECTED [reason=%d (%s)]", 
		  inv->cause,
		  pjsip_get_status_text(inv->cause)->ptr));

	PJ_LOG(3,(THIS_FILE, "One call completed, application quitting..."));
	g_complete = 1;

    pjsip_inv_end_session (g_inv, 200, NULL, &tdata);

    } else {

	PJ_LOG(3,(THIS_FILE, "\nCall state changed to %s\n", 
		  pjsip_inv_state_name(inv->state)));

    }

    
}


/* This callback is called when dialog has forked. */
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e)
{
    /* To be done... */
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);
}


/*
 * Callback when incoming requests outside any transactions and any
 * dialogs are received. We're only interested to hande incoming INVITE
 * request, and we'll reject any other requests with 500 response.
 */
static pj_bool_t on_rx_request( pjsip_rx_data *rdata )
{
    pj_sockaddr hostaddr;
    char temp[80], hostip[PJ_INET6_ADDRSTRLEN];
    pj_str_t local_uri;
    pjsip_dialog *dlg;
    pjmedia_sdp_session *local_sdp;
    
    unsigned options = 0;
    pj_status_t status;


    
     // Respond (statelessly) any non-INVITE requests with 500 
     
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {

	if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
	    pj_str_t reason = pj_str("Simple UA unable to handle "
				     "this request");

	    pjsip_endpt_respond_stateless( g_endpt, rdata, 
					   500, &reason,
					   NULL, NULL);
	}
	return PJ_TRUE;
    }


    
     // Reject INVITE if we already have an INVITE session in progress.
   
    if (g_inv) {

	pj_str_t reason = pj_str("Another call is in progress");

	pjsip_endpt_respond_stateless( g_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
	return PJ_TRUE;

    }

   // Verify that we can handle the request. 
    status = pjsip_inv_verify_request(rdata, &options, NULL, NULL,
				      g_endpt, NULL);
    if (status != PJ_SUCCESS) {

	pj_str_t reason = pj_str("Sorry Simple UA can not handle this INVITE");

	pjsip_endpt_respond_stateless( g_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
	return PJ_TRUE;
    } 

    
     //Generate Contact URI
     
    if (pj_gethostip(AF, &hostaddr) != PJ_SUCCESS) {
	//pj_perror(THIS_FILE, "Unable to retrieve local host IP", status);
	return PJ_TRUE;
    }
    pj_sockaddr_print(&hostaddr, hostip, sizeof(hostip), 2);

    pj_ansi_sprintf(temp, "<sip:simpleuas@%s:%d>", 
		    hostip, SIP_PORT);
    local_uri = pj_str(temp);

   // Create UAS dialog.
    
    status = pjsip_dlg_create_uas_and_inc_lock( pjsip_ua_instance(),
						rdata,
						&local_uri, // contact 
						&dlg);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_respond_stateless(g_endpt, rdata, 500, NULL,
				      NULL, NULL);
	return PJ_TRUE;
    }

     
     // Get media capability from media endpoint: 
    

    status = pjmedia_endpt_create_sdp( g_med_endpt, rdata->tp_info.pool,
				       MAX_MEDIA_CNT, g_sock_info, &local_sdp);
    pj_assert(status == PJ_SUCCESS);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_dec_lock(dlg);
	return PJ_TRUE;
    }


    /* 
     * Create invite session, and pass both the UAS dialog and the SDP
     * capability to the session.
     */
    status = pjsip_inv_create_uas( dlg, rdata, local_sdp, 0, &g_inv);
    pj_assert(status == PJ_SUCCESS);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_dec_lock(dlg);
	return PJ_TRUE;
    }

    
     //Invite session has been created, decrement & release dialog lock.
    
    pjsip_dlg_dec_lock(dlg);

    cdlg = dlg;

    /*
     * Initially send 180 response.
     *
     * The very first response to an INVITE must be created with
     * pjsip_inv_initial_answer(). Subsequent responses to the same
     * transaction MUST use pjsip_inv_answer().
     */

    status = pjsip_inv_initial_answer(g_inv, rdata, 
				      100, 
				      NULL, NULL, &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);
    // Send the 100 response.   
    status = pjsip_inv_send_msg(g_inv, tdata); 
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);


    status = pjsip_inv_answer
                (
                    g_inv,
                    180, 
                    NULL, NULL, &tdata
                );
    
    
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);
    // Send the 180 response.   
    status = pjsip_inv_send_msg(g_inv, tdata); 
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);
    
    //g2_inv = g_inv;
    //r2data = rdata;

    delay.sec = 5;
    delay.msec = 0;
    pj_timer_heap_schedule(timer, &entry[0], &delay);

    delay.sec = 15;
    pj_timer_heap_schedule(timer, &entry[1], &delay);


     
     //When the call is disconnected, it will be reported via the callback.
     

    return PJ_TRUE;
}

 

/*
 * Callback when SDP negotiation has completed.
 * We are interested with this callback because we want to start media
 * as soon as SDP negotiation is completed.
 */
static void call_on_media_update( pjsip_inv_session *inv,
				  pj_status_t status)
{
    pjmedia_stream_info stream_info;
    const pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *remote_sdp;

    if (status != PJ_SUCCESS) {


	/* Here we should disconnect call if we're not in the middle 
	 * of initializing an UAS dialog and if this is not a re-INVITE.
	 */
	return;
    }

    /* Get local and remote SDP.
     * We need both SDPs to create a media session.
     */
    status = pjmedia_sdp_neg_get_active_local(inv->neg, &local_sdp);

    status = pjmedia_sdp_neg_get_active_remote(inv->neg, &remote_sdp);


    // Create stream info based on the media audio SDP. 
    status = pjmedia_stream_info_from_sdp(&stream_info, inv->dlg->pool,
					  g_med_endpt,
					  local_sdp, remote_sdp, 0);
    if (status != PJ_SUCCESS) {
	
	return;
    }

    /* If required, we can also change some settings in the stream info,
     * (such as jitter buffer settings, codec settings, etc) before we
     * create the stream.
     */

    /* Create new audio media stream, passing the stream info, and also the
     * media socket that we created earlier.
     */
    status = pjmedia_stream_create(g_med_endpt, inv->dlg->pool, &stream_info,
				   g_med_transport[0], NULL, &g_med_stream);
    if (status != PJ_SUCCESS) {
	    return;
    }

    // Start the audio stream 
    status = pjmedia_stream_start(g_med_stream);
    if (status != PJ_SUCCESS) {
	return;
    }
	
    // Start the UDP media transport 
    pjmedia_transport_media_start(g_med_transport[0], 0, 0, 0, 0);

    
    pjmedia_stream_get_port(g_med_stream, &stream_port);

    /*if (out_port == NULL)
    {
        pjmedia_tonegen_create(pool, 8000, 1, 160, 16, 0, &tonegen_port);

	    pjmedia_tone_desc tone[1];
        tone[0].freq1 = 400;
	    tone[0].freq2 = 0;
	    tone[0].on_msec = 400;
	    tone[0].off_msec = 100;

	    pjmedia_stream_info st_i;
	    pjmedia_stream_get_info (g_med_stream, &st_i);

	    /*if (stream_port->info.fmt.det.aud.clock_rate == PJMEDIA_TYPE_AUDIO)  //здесь надо проверить совпадают ли тактовые частоты медиа порта стрима и тонгена 
	    {
		printf ("\n\nTaki audio\n\n");
		getchar();
	    } 
	    printf ("\n\nstream=%u tonegen=%u\n\n", 
			PJMEDIA_PIA_SPF (&stream_port->info),
			PJMEDIA_PIA_SPF (&tonegen_port->info) 
			);
			//stream_port->info.fmt.det.aud.,
			//tonegen_port->info.fmt.det.aud.clock_rate);
	    //getchar(); 
        pjmedia_tonegen_play(tonegen_port, 1, tone, PJMEDIA_TONEGEN_LOOP);
        //out_port = tonegen_port;
    } */

    pjmedia_audio_format_detail *stream_afd, *out_afd;
    stream_afd = pjmedia_format_get_audio_format_detail(&stream_port->info.fmt, PJ_TRUE);
    out_afd = pjmedia_format_get_audio_format_detail(&out_port->info.fmt, PJ_TRUE);
    

	pjmedia_master_port_create (pool, out_port, stream_port, 0, &mp);
    
	pjmedia_master_port_start (mp);

    // Done with media. 
}

int main(int argc, char *argv[])
{
    
    pj_status_t status;
    unsigned i;

    // Must init PJLIB first: 
    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    pj_log_set_level(5);

    // Then init PJLIB-UTIL: 
    status = pjlib_util_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    // Must create a pool factory before we can allocate any memory. 
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

	pool = pj_pool_create( &cp.factory,	    // pool factory	    
			   "app",	    // pool name.	    
			   4000,	    // init size	    
			   4000,	    // increment size	    
			   &poolfail_cb		    // callback on error    
			   );

    // Create global endpoint: 
    {
	const pj_str_t *hostname;
	const char *endpt_name;

	/* Endpoint MUST be assigned a globally unique name.
	 * The name will be used as the hostname in Warning header.
	 */

	// For this implementation, we'll use hostname for simplicity 
	hostname = pj_gethostname();
	endpt_name = hostname->ptr;

	// Create the endpoint: 

	status = pjsip_endpt_create(&cp.factory, endpt_name, 
				    &g_endpt);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }


    /* 
     * Add UDP transport, with hard-coded port 
     * Alternatively, application can use pjsip_udp_transport_attach() to
     * start UDP transport, if it already has an UDP socket (e.g. after it
     * resolves the address with STUN).
     */
    {
	pj_sockaddr addr;

	pj_sockaddr_init(AF, &addr, NULL, (pj_uint16_t)SIP_PORT);
	
	if (AF == pj_AF_INET()) {
	    status = pjsip_udp_transport_start( g_endpt, &addr.ipv4, NULL, 
						1, NULL);
	} else if (AF == pj_AF_INET6()) {
	    status = pjsip_udp_transport_start6(g_endpt, &addr.ipv6, NULL,
						1, NULL);
	} else {
	    status = PJ_EAFNOTSUP;
	}

	if (status != PJ_SUCCESS) {
	    return 1;
	}
    }


    /* 
     * Init transaction layer.
     * This will create/initialize transaction hash tables etc.
     */
    status = pjsip_tsx_layer_init_module(g_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* 
     * Initialize UA layer module.
     * This will create/initialize dialog hash tables etc.
     */
    status = pjsip_ua_init_module( g_endpt, NULL );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* 
     * Init invite session module.
     * The invite session module initialization takes additional argument,
     * i.e. a structure containing callbacks to be called on specific
     * occurence of events.
     *
     * The on_state_changed and on_new_session callbacks are mandatory.
     * Application must supply the callback function.
     *
     * We use on_media_update() callback in this application to start
     * media transmission.
     */
    {
	pjsip_inv_callback inv_cb;

	// Init the callback for INVITE session: 
	pj_bzero(&inv_cb, sizeof(inv_cb));
	inv_cb.on_state_changed = &call_on_state_changed;
	inv_cb.on_new_session = &call_on_forked;
	inv_cb.on_media_update = &call_on_media_update;

	// Initialize invite session module:  
	status = pjsip_inv_usage_init(g_endpt, &inv_cb);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }

    // Initialize 100rel support 
    status = pjsip_100rel_init_module(g_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    
     //* Register our module to receive incoming requests.
     
    status = pjsip_endpt_register_module( g_endpt, &mod_simpleua);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    
     // Register message logger module.
     
    status = pjsip_endpt_register_module( g_endpt, &msg_logger);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* 
     * Initialize media endpoint.
     * This will implicitly initialize PJMEDIA too.
     */

    status = pjmedia_endpt_create(&cp.factory, NULL, 1, &g_med_endpt);


    

    status = pjmedia_codec_g711_init(g_med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    // Create event manager 
    status = pjmedia_event_mgr_create(pool, 0, NULL);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* 
     * Create media transport used to send/receive RTP/RTCP socket.
     * One media transport is needed for each call. Application may
     * opt to re-use the same media transport for subsequent calls.
     */
    for (i = 0; i < PJ_ARRAY_SIZE(g_med_transport); ++i) {
	status = pjmedia_transport_udp_create3(g_med_endpt, AF, NULL, NULL, 
					       RTP_PORT + i*2, 0, 
					       &g_med_transport[i]);
	if (status != PJ_SUCCESS) {
	    
	    return 1;
	}

	/* 
	 * Get socket info (address, port) of the media transport. We will
	 * need this info to create SDP (i.e. the address and port info in
	 * the SDP).
	 */
	pjmedia_transport_info_init(&g_med_tpinfo[i]);
	pjmedia_transport_get_info(g_med_transport[i], &g_med_tpinfo[i]);

	pj_memcpy(&g_sock_info[i], &g_med_tpinfo[i].sock_info,
		  sizeof(pjmedia_sock_info));
    }

    status = pjmedia_wav_player_port_create 
                 (  
                          pool,	// memory pool	    
					      "task2.wav",	// file to play	 (16 bitrate, 8 khz, mono)
					      20,	// ptime.	    
					      0,	// flags	    
					      0,	// default buffer   
					      &player_port// returned port    
				 );
        if (PJ_SUCCESS != status)
        {
            pj_perror (5, "wav pjmedia create", status, "egog");
            exit (1);
        }
        out_port = player_port;


	PJ_LOG(5,(THIS_FILE, "Ready to accept incoming calls..."));
    


    
    
    pj_timer_heap_create(pool, 1, &timer);

    pj_timer_entry_init (&entry[0], 0, NULL, &pjtimer_callback);
    pj_timer_entry_init (&entry[1], 1, NULL, &auto_exit);

    while ( !g_complete )
    {
        pj_time_val timeout = {0, 10};
	    pjsip_endpt_handle_events(g_endpt, &timeout);
        
        pj_timer_heap_poll(timer, NULL);

    }

    pjsip_inv_end_session (g_inv, 200, NULL, &tdata);

    if (mp) 
        pjmedia_master_port_destroy (mp, 0);


    if (sound_port)
	pjmedia_snd_port_destroy(sound_port);



    // Destroy streams 
    if (g_med_stream)
	pjmedia_stream_destroy(g_med_stream);


    // Destroy media transports 
    for (i = 0; i < MAX_MEDIA_CNT; ++i) {
	if (g_med_transport[i])
	    pjmedia_transport_close(g_med_transport[i]);
    }

    // Destroy event manager 
    pjmedia_event_mgr_destroy(NULL); 

    // Deinit pjmedia endpoint 
    if (g_med_endpt)
	pjmedia_endpt_destroy(g_med_endpt);

    // Deinit pjsip endpoint 
    if (g_endpt)
	pjsip_endpt_destroy(g_endpt);

    // Release pool 
    if (pool)
	pj_pool_release(pool);

    return 0;
}
