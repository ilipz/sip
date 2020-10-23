#include <pjsip.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

// For logging purpose. 
#define THIS_FILE   "TASK 3.3"

// Settings 
#define AF		pj_AF_INET() 

#define SIP_PORT	5060	     // Listening SIP port		
#define RTP_PORT	4000	     // RTP port			
#define MAX_MEDIA_CNT	1	     // Media count, set to 1 for audio (ERASE)

////////////Unused (i.e. out of using) funcs//////////////////////
static pj_bool_t logging_on_rx_msg(pjsip_rx_data *rdata);
static pj_status_t logging_on_tx_msg(pjsip_tx_data *tdata);
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e);
static pj_bool_t on_rx_request( pjsip_rx_data *rdata );

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





/////////////Custom vars/////////////////////////////////////
pjsip_endpoint  *sip_endpt;

pj_caching_pool cp; // Global pool factory.	
pj_pool_t       *pool = NULL; // pool for tonegen, .wav player etc.

pj_bool_t g_complete = PJ_TRUE;

pjmedia_port    *tonegen_port=NULL;
pjmedia_port    *player_port=NULL;

pjmedia_endpt     *media_endpt;

//pj_timer_entry  entry[2]; // in slot_t ?
pj_timer_heap_t *timer;
const pj_time_val     delay1 = {10, 0};
const pj_time_val     delay2 = {10, 0};
//pj_bool_t slots_st[20];
uint8_t slots_count=20;
pj_mutex_t *dec_mutex, *inc_mutex, *mutex;

#define SLOTS_Q 19

enum state_t
{
    WAITING,
    RINGING,
    SPEAKING
};

typedef struct slot_t 
{
    enum state_t state;
    pjmedia_transport   *media_transport; // on_rx_request
    pjmedia_stream      *media_stream; // call_on_media_update
    pjmedia_port        *stream_port; // call_on_media_update
    pjmedia_master_port *mp; // call_on_media_update
    //pjmedia_endpt       *media_endpt;
    pjmedia_sock_info   *media_sock_info;

    pjsip_inv_session   *inv_ss; // on_rx_request
    pjsip_dialog        *dlg; // on_rx_request

    pjsip_tx_data       *tdata; // on_rx_request
    pjsip_rx_data       *rdata; // on_rx_request

    pj_pool_t           *ss_pool; // on_rx_request

    pj_timer_heap_t     *timer_heap; // heap for 2 timers
    pj_timer_entry      entry[2]; // 0 for accept_call(), 1 for auto_exit()

    pj_bool_t           busy;
    
   
} slot_t;


slot_t slots[20];

// My custom funcs
void when_exit (int none);
void accept_call(pj_timer_heap_t *ht, pj_timer_entry *e);
void auto_exit (pj_timer_heap_t *ht, pj_timer_entry *e);
void poolfail_cb (pj_pool_t *pool, pj_size_t size);
void nullize_slot (slot_t *slot);
void free_slot_by_inv (pjsip_inv_session *inv);
int get_index_by_inv (pjsip_inv_session *inv); 
slot_t* get_slot ();
void free_slot (slot_t *slot);
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

	PJ_LOG(3,(THIS_FILE, "One call completed, wait next one..."));

    free_slot_by_inv (inv);

    
    
    //not_first = PJ_TRUE;
    
    //g_inv = NULL;

    } else {

	PJ_LOG(3,(THIS_FILE, "\nCall state changed to %s\n", 
		  pjsip_inv_state_name(inv->state)));

    }    
}

/*
 * Callback when incoming requests outside any transactions and any
 * dialogs are received. We're only interested to hande incoming INVITE
 * request, and we'll reject any other requests with 500 response.
 */

// Also register slots (fill this fields): make media_transport, pjsip_inv, dialog,
// rdata, tdata, session pool, timers (create timer heap, 2 timer entries). 
// Sets slot as busy;
// Starts: timer for 200 OK


static pj_bool_t on_rx_request( pjsip_rx_data *rdata ) 
{
    printf ("\n\n\nON_RX_REQUEST()\n\n\n");
    pj_sockaddr hostaddr;
    
    char temp[80], hostip[PJ_INET6_ADDRSTRLEN];
    pj_str_t local_uri;
    pjsip_dialog *dlg;
    pjmedia_sdp_session *local_sdp;
    pjsip_inv_session *inv;
    pjsip_tx_data *tdata=NULL;
    
    unsigned options = 0;
    pj_status_t status;
    
    // get free slot

    
    
    
     // Respond (statelessly) any non-INVITE requests with 500 
     
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) {

	if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
	    pj_str_t reason = pj_str("TASK 3.3 unable to handle "
				     "this request");

	    pjsip_endpt_respond_stateless( sip_endpt, rdata, 
					   500, &reason,
					   NULL, NULL);
	}
	return PJ_TRUE;
    }


    
     // Reject INVITE if we already have an INVITE session in progress.
    
    slot_t *tmp = get_slot ();
    
    if (tmp == NULL) {

	pj_str_t reason = pj_str("Busy here");

	pjsip_endpt_respond_stateless( sip_endpt, rdata, 
				       486, &reason,
				       NULL, NULL);
	return PJ_TRUE;

    }
    

   // Verify that we can handle the request. 
    status = pjsip_inv_verify_request(rdata, &options, NULL, NULL,
				      sip_endpt, NULL);
    if (status != PJ_SUCCESS) {

	pj_str_t reason = pj_str("Sorry Simple UA can not handle this INVITE");

	pjsip_endpt_respond_stateless( sip_endpt, rdata, 
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
	pjsip_endpt_respond_stateless(sip_endpt, rdata, 500, NULL,
				      NULL, NULL);
	return PJ_TRUE;
    }

     
     // Get media capability from media endpoint: 
    
      
    pjmedia_sock_info   media_sock_info;
    pjmedia_transport *media_transport; 
    
    status = pjmedia_transport_udp_create3(media_endpt, AF, NULL, NULL, 
					       RTP_PORT + 1 + slots_count*2, 0, 
					       &media_transport);
	if (status != PJ_SUCCESS) {
	    
	    return 1;
	}

    pjmedia_transport_info mti;
	pjmedia_transport_info_init(&mti);
	pjmedia_transport_get_info(media_transport, &mti);

	pj_memcpy(&media_sock_info, &mti.sock_info,
		  sizeof(pjmedia_sock_info));
    
    status = pjmedia_endpt_create_sdp(media_endpt, dlg->pool,
				       1, &media_sock_info, &local_sdp);
    pj_assert(status == PJ_SUCCESS);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_dec_lock(dlg);
	return PJ_TRUE;
    }


    /* 
     * Create invite session, and pass both the UAS dialog and the SDP
     * capability to the session.
     */
    status = pjsip_inv_create_uas( dlg, rdata, local_sdp, 0, &inv);
    pj_assert(status == PJ_SUCCESS);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_dec_lock(dlg);
	return PJ_TRUE;
    }

    
     //Invite session has been created, decrement & release dialog lock.
    
    
    // Register slot
    tmp->inv_ss = inv;
    tmp->dlg = dlg;
    tmp->rdata = rdata;
    tmp->tdata = tdata;
    tmp->ss_pool = tmp->inv_ss->pool; 
    tmp->media_sock_info = &media_sock_info;
    //tmp->media_endpt = media_endpt;
    tmp->media_transport = media_transport;
    
    pj_timer_heap_create (tmp->ss_pool, 2, &tmp->timer_heap);

    pj_timer_entry_init (&tmp->entry[0], 0, (void*)tmp, &accept_call);
    pj_timer_entry_init (&tmp->entry[1], 1, (void*)tmp, &auto_exit);

    
    /*
     * Initially send 180 response.
     *
     * The very first response to an INVITE must be created with
     * pjsip_inv_initial_answer(). Subsequent responses to the same
     * transaction MUST use pjsip_inv_answer().
     */

    status = pjsip_inv_initial_answer(tmp->inv_ss, rdata, 
				      100, 
				      NULL, NULL, &tdata);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);
    // Send the 100 response.   
    status = pjsip_inv_send_msg(tmp->inv_ss, tdata); 
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

    
    tmp->state = RINGING;
    

    
    status = pjsip_inv_answer
                (
                    tmp->inv_ss,
                    183,                
                    NULL, NULL, &tdata
                );
    
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);
    // Send the 180 response.   
    status = pjsip_inv_send_msg(tmp->inv_ss, tdata); 
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE);

    
    
    
    

     
     //When the call is disconnected, it will be reported via the callback.

    

    //g_inv = inv;
    

    

    return PJ_TRUE;
}



/*
 * Callback when SDP negotiation has completed.
 * We are interested with this callback because we want to start media
 * as soon as SDP negotiation is completed.
 */

// Also register slots: master port, stream port, stream
// Starts: master port, timer for exit
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
    exit (2);
	return;
    }
    int index = get_index_by_inv (inv);
    if (index == -1)
        exit(1);
    
    //sleep(20);
    slot_t *tmp = &slots[index];

    /* Get local and remote SDP.
     * We need both SDPs to create a media session.
     */
    status = pjmedia_sdp_neg_get_active_local(tmp->inv_ss->neg, &local_sdp);

    status = pjmedia_sdp_neg_get_active_remote(tmp->inv_ss->neg, &remote_sdp);


    // Create stream info based on the media audio SDP. 
    status = pjmedia_stream_info_from_sdp(&stream_info, tmp->ss_pool,
					  media_endpt,
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
    status = pjmedia_stream_create(media_endpt, tmp->ss_pool, &stream_info,
				   tmp->media_transport, NULL, &tmp->media_stream);
    if (status != PJ_SUCCESS) {
	    return;
    }

    // Start the audio stream 
    status = pjmedia_stream_start(tmp->media_stream);
    if (status != PJ_SUCCESS) {
	return;
    }
	
    // Start the UDP media transport 
    status = pjmedia_transport_media_start(tmp->media_transport, 0, 0, 0, 0);

    if (PJ_SUCCESS != status) 
        pj_perror (5,THIS_FILE, status, "on_media_upd");
    
    status = pjmedia_stream_get_port(tmp->media_stream, &tmp->stream_port);
    if (PJ_SUCCESS != status) 
        pj_perror (5,THIS_FILE, status, "on_media_upd");

    status = pjmedia_master_port_create (tmp->ss_pool, tonegen_port, tmp->stream_port, 
                                0, &tmp->mp); 
    if (PJ_SUCCESS != status) 
        pj_perror (5,THIS_FILE, status, "on_media_upd"); 

	status = pjmedia_master_port_start (tmp->mp);
    if (PJ_SUCCESS != status) 
        pj_perror (5,THIS_FILE, status, "on_media_upd");
    pj_timer_heap_schedule(tmp->timer_heap, &tmp->entry[0], &delay1); // start timer to accept

    //pj_timer_heap_schedule(tmp->timer_heap, &tmp->entry[1], &delay);


    // Done with media. 
}



/////////////////////////////////////////////////////////////////////////////////////
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

    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

	pool = pj_pool_create( &cp.factory,	    // pool factory	    
			   "app",	    // pool name.	    
			   4000,	    // init size	    
			   4000,	    // increment size	    
			   &poolfail_cb		    // callback on error    
			   );
    // Init slots
    for (pj_uint8_t i2=0; i2<20; i2++)
    {
        nullize_slot (&slots[i2]);
        //slots_st[i2] = PJ_FALSE;
        /*status = pj_mutex_create (pool, s, PJ_MUTEX_SIMPLE, &slots[i2].mutex) //pj_sem_create (pool, s, 1, 2, &slots[i2].sem);
        if (status != PJ_SUCCESS)
        {
            pj_perror (5, "main()", status, "error creating mutex");
            exit (1);
        } */
    }

    //pj_mutex_create (pool, "global slots_count-- mutex", PJ_MUTEX_SIMPLE, &inc_mutex);
    pj_mutex_create (pool, "global slots_count mutex", PJ_MUTEX_SIMPLE, &mutex);
    // Must create a pool factory before we can allocate any memory. 
    
    
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
				    &sip_endpt);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }


    /* 
     * Add UDP transport, with hard-coded port 
     * Alternatively, application can use pjsip_udp_transport_attach() to
     * start UDP transport, if it already has an UDP socket (e.g. after it
     * resolves the address with STUN).
     */
    
	pj_sockaddr addr;


	
    pj_sockaddr_init(AF, &addr, NULL, (pj_uint16_t)SIP_PORT);
	
	if (AF == pj_AF_INET()) 
	    status = pjsip_udp_transport_start( sip_endpt, &addr.ipv4, NULL, 
						1, NULL);
	 else 
	    status = PJ_EAFNOTSUP;
	

	if (status != PJ_SUCCESS) 
	    return 1;


    /* 
     * Init transaction layer.
     * This will create/initialize transaction hash tables etc.
     */
    status = pjsip_tsx_layer_init_module(sip_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* 
     * Initialize UA layer module.
     * This will create/initialize dialog hash tables etc.
     */
    status = pjsip_ua_init_module( sip_endpt, NULL );
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
	status = pjsip_inv_usage_init(sip_endpt, &inv_cb);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }

    // Initialize 100rel support 
    status = pjsip_100rel_init_module(sip_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    
     // Register our module to receive incoming requests.
     
    status = pjsip_endpt_register_module( sip_endpt, &mod_simpleua);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    
     // Register message logger module.
     
    status = pjsip_endpt_register_module( sip_endpt, &msg_logger);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* 
     * Initialize media endpoint.
     * This will implicitly initialize PJMEDIA too.
     */

    status = pjmedia_endpt_create(&cp.factory, NULL, 1, &media_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    status = pjmedia_codec_g711_init(media_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    // Create event manager 
    status = pjmedia_event_mgr_create(pool, 0, NULL);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    
    /* 
     * Create media transport used to send/receive RTP/RTCP socket.
     * One media transport is needed for each call. Application may
     * opt to re-use the same media transport for subsequent calls.
     */
    

	
    
    pjmedia_tonegen_create(pool, 8000, 1, 160, 16, 0, &tonegen_port);

    pjmedia_tone_desc tone[1];
    tone[0].freq1 = 400;
    tone[0].freq2 = 0;
    tone[0].on_msec = 1000;
    tone[0].off_msec = 4000;

    pjmedia_tonegen_play(tonegen_port, 1, tone, PJMEDIA_TONEGEN_LOOP);

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

   

    PJ_LOG(5,(THIS_FILE, "Ready to accept incoming calls..."));

    signal (SIGINT, when_exit);
/////////////////////////////////////////////////////////////////////////////
    while ( g_complete )
    {
        pj_time_val timeout = {0, 10};
	    pjsip_endpt_handle_events(sip_endpt, &timeout);
        printf ("\n\n////////////////////\n\n>> Available: %d <<\n\n////////////////////\n\n", slots_count);
        for (int i=0; i<20; i++)
            if (slots[i].busy)
                pj_timer_heap_poll (slots[i].timer_heap, NULL);

    }
/////////////////////////////////////////////////////////////////////////////

    /*for (int i=0; i<20; i++)
    {
        pj_mutex_destroy (slots[i].mutex);
    } */
    for (int i=0; i<20; i++)
        free_slot_by_inv (slots[i].inv_ss);
    
    /*if (g_inv);
        //pjsip_inv_end_session (g_inv, 200, NULL, &tdata);

    if (mp) 
        pjmedia_master_port_destroy (mp, 0);


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
	pjmedia_endpt_destroy(g_med_endpt); */

    // Deinit pjsip endpoint 
    
    if (sip_endpt)
	pjsip_endpt_destroy(sip_endpt);

    // Release pool 
    if (pool)
	pj_pool_release(pool);

    return 0;
}

/////////Custom and unused////////////////////////////

/////////Custom funcs/////////////////////////////

slot_t * get_slot ()
{
    pj_mutex_lock (mutex);
    for (int i=0; i<20; i++)
        if (!slots[i].busy)
        {
            slots[i].busy = PJ_TRUE;
            slots_count--;
            pj_mutex_unlock (mutex);
            return &slots[i];
        }
    pj_mutex_unlock (mutex);
    return NULL;
}

/*void free_slot (slot_t * slot)
{
    pj_mutex_lock (mutex);
    slot->busy = PJ_FALSE;
    pj_mutex_unlock (mutex);
}*/

void when_exit (int none)
{
    PJ_UNUSED_ARG(none);
    g_complete = PJ_FALSE;
}

void nullize_slot (slot_t *slot)
{
    //slot->sem = NULL;
    slot->state = WAITING;
    slot->media_transport = NULL;
    slot->media_stream = NULL;
    slot->stream_port = NULL;
    slot->mp = NULL;
    //slot->media_endpt = NULL;
    slot->media_sock_info = NULL;
    slot->inv_ss = NULL;
    slot-> dlg = NULL;
    slot-> tdata = NULL;
    slot->rdata = NULL;
    slot->ss_pool = NULL;
    for (int i=0; i<2; i++)
    {
        slot->entry[i].cb = NULL;
        slot->entry[i].id = -1;
        slot->entry[i].user_data = NULL;
        slot->entry[i]._timer_id = 0;
    }
    
    slot->busy = PJ_FALSE;
}

int get_index_by_inv (pjsip_inv_session *inv)
{
    for (int i=0; i<20; i++)
    {
        if (slots[i].inv_ss == inv)
        {
            return i;
        } 
    }
    return -1;
}

void free_slot_by_inv (pjsip_inv_session *inv)
{
    int i = get_index_by_inv (inv);
    
    if (i == -1)
        return;
    PJ_LOG (5, (THIS_FILE, "!!! Destroying slot#%d", i));
    //pjmedia_master_port_stop (slots[i].mp);

    if (slots[i].timer_heap)
        pj_timer_heap_destroy (slots[i].timer_heap);
    if (slots[i].mp)
        pjmedia_master_port_destroy (slots[i].mp, PJ_FALSE);
    if (slots[i].media_stream)
        pjmedia_stream_destroy (slots[i].media_stream);
    if (slots[i].media_transport)
        pjmedia_transport_close (slots[i].media_transport);
    if (slots[i].dlg)
    {
        pjsip_tx_data *request_data=NULL;
        pjsip_method *method=NULL;
        switch (slots[i].state)
        {
            case WAITING: break;
            case SPEAKING: method = &pjsip_bye_method; break; 
            case RINGING: method = &pjsip_cancel_method; break;
            default: exit(5);
        }
        if (method)
        {
            pjsip_dlg_create_request (slots[i].dlg, method, -1, &request_data);
            pjsip_dlg_send_request (slots[i].dlg, request_data, -1, NULL);
        }
        
        
    } 
    
    //pj_sem_post (slots[i].sem);
    
    if (slots[i].inv_ss)
        pjsip_inv_end_session (slots[i].inv_ss, 200, NULL, &slots[i].tdata);

    nullize_slot (&slots[i]);
    slots[i].busy = PJ_FALSE;
    slots_count++;
}


// Sends 200 OK
// Switch master's port u_port from tonegen to player_port
// Starts timer for exits
void accept_call(pj_timer_heap_t *ht, pj_timer_entry *e) // callback
{
    // ! Switch global vars to slot's one

    pj_status_t status;
    PJ_UNUSED_ARG(ht);
    //uint8_t *index = (uint8_t*) ;
    slot_t *tmp = (slot_t*)e->user_data;
    if (!tmp->busy)
        return;
    
    status =   pjsip_inv_answer
                (
                    tmp->inv_ss, 200, NULL, NULL, &tmp->tdata 
                );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, PJ_TRUE); 
    
    tmp->state = SPEAKING;
    
    /*pjmedia_master_port_stop(tmp->mp);
    pjmedia_master_port_set_uport (tmp->mp, player_port); 
    pjmedia_master_port_start (tmp->mp); */
    status = pjsip_inv_send_msg(tmp->inv_ss, tmp->tdata);
   
    pj_timer_heap_schedule(tmp->timer_heap, &tmp->entry[1], &delay2);
}

// Sends BYE, 
void auto_exit (pj_timer_heap_t *ht, pj_timer_entry *e) // callback
{
    PJ_UNUSED_ARG(ht);
    //uint8_t *index = (uint8_t*) e->user_data;
    slot_t *tmp = (slot_t*)e->user_data;
    //PJ_UNUSED_ARG(e);
    //pjmedia_master_port_stop (tmp->mp);
    if (tmp->busy)
        free_slot_by_inv (tmp->inv_ss);
    //pjsip_inv_end_session (tmp->inv_ss, 200, NULL, &tmp->tdata);
    /*pjsip_tx_data *bye_data;
    pjsip_dlg_create_request (cdlg, &pjsip_bye_method, -1, &bye_data);
    pjsip_dlg_send_request (cdlg, bye_data, -1, NULL); */  

}

// prosto tak dobavil
void poolfail_cb (pj_pool_t *pool, pj_size_t size)
{
    pj_perror (5, "app", 0, "Ploho" );
    exit (1);
}

//////////////////////////////////////////////////////////////////////////////////

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

/* This callback is called when dialog has forked. */
static void call_on_forked(pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_LOG (3, (THIS_FILE, "\n\n!!!DIALOG FORKED!!!\n\n"));
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);
}