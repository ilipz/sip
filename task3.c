/*
    TASK 3
*/

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
#include <unistd.h>
#include <getopt.h>
// Settings 
#define PJ_LOG_ERROR "!!! ERROR: " 
#define APPNAME     "TASK 3:"

pj_uint16_t SIP_PORT = 7060;	     // Listening SIP port		
pj_uint16_t RTP_PORT = 9000;	     // RTP port			

////////////////// VARS ///////////////////////
pjsip_endpoint      *sip_endpt; //pjsip_endpt_get_timer_heap

pj_caching_pool     cp; // Global pool factory.	
pj_pool_t           *pool = NULL; // pool for tonegen, .wav player etc.

pj_bool_t           g_complete = PJ_TRUE;

pjmedia_port        *ringback_tonegen_port=NULL;
pjmedia_port        *station_answer_tonegen_port=NULL;
pjmedia_port        *quick_beep_tonegen_port=NULL;
pjmedia_port        *warning_tonegen_port=NULL;
pjmedia_port        *player_port=NULL;

unsigned            ringback_conf_id, station_answer_conf_id, 
                    quick_beep_conf_id, warning_conf_id, player_conf_id;

pjmedia_master_port *conf_mp;

pjmedia_endpt       *media_endpt;


const pj_time_val   delay1 = {3, 0};
const pj_time_val   delay2 = {40, 0};
pj_uint32_t            timer_count=0;

pjmedia_conf        *conf=NULL;

pj_uint8_t          slots_count=20;

pj_bool_t           pause1=PJ_FALSE;
pj_bool_t           to_halt=PJ_FALSE;
pj_bool_t           to_exit=PJ_FALSE;

pj_timer_heap_t     *timer_heap=NULL;


///////// SLOT REFERENCE /////////////////////
typedef struct slot_t 
{
    pjmedia_transport   *media_transport; // on_rx_request
    pjmedia_stream      *media_stream; // call_on_media_update
    pjmedia_port        *stream_port; // call_on_media_update
    
    pjmedia_master_port *mp; // call_on_media_update
    pjmedia_sock_info   *media_sock_info;

    pjsip_inv_session   *inv_ss; // on_rx_request
    pjsip_dialog        *dlg; // on_rx_request

    pjsip_tx_data       *tdata; // on_rx_request
    pjsip_rx_data       *rdata; // on_rx_request

    pj_pool_t           *ss_pool; // on_rx_request

    pj_timer_entry      entry[2]; // 0 for accept_call(), 1 for auto_exit()
    pj_uint16_t         entry_id[2];

    pj_mutex_t          *mutex;
    pj_bool_t           busy;

    char                telephone[64];
    char                uri[64];

    unsigned            conf_id;    
    unsigned            input_port;
    int                 index;
   
} slot_t;


slot_t slots[20];

//////////// FUNCTIONS //////////////////////

// Simple UA Callbacks
static pj_bool_t    logging_on_rx_msg       (pjsip_rx_data *rdata);
static pj_status_t  logging_on_tx_msg       (pjsip_tx_data *tdata);

static pj_bool_t    on_rx_request           (pjsip_rx_data *rdata);
static void         call_on_state_changed   (pjsip_inv_session *inv, pjsip_event *e);
static void         call_on_media_update    (pjsip_inv_session *inv, pj_status_t status);

static void         call_on_forked          (pjsip_inv_session *inv, pjsip_event *e);

// Task 3 callbacks
void                accept_call             (pj_timer_heap_t *ht, pj_timer_entry *e);
void                auto_exit               (pj_timer_heap_t *ht, pj_timer_entry *e);
void                poolfail_cb             (pj_pool_t *pool, pj_size_t size);

// Slots funcs
void                nullize_slot            (slot_t *slot);
void                free_slot_by_inv        (pjsip_inv_session *inv);
int                 get_slot_by_inv         (pjsip_inv_session *inv); 
void                free_slot               (slot_t *slot);
slot_t*             get_slot                ();


// Other
int                 thread_func             (void *p);
void free_all_slots ();
void emergency_exit ();
void halt ();
void destroy_slot (slot_t *slot);

////////// The module instance /////////////// 
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


/*
 * Callback when INVITE session state has changed.
 * This callback is registered when the invite session module is initialized.
 * We mostly want to know when the invite session has been disconnected,
 * so that we can quit the application.
 */

static void call_on_state_changed (pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(e);
    const char THIS_FUNCTION[] = "call_on_state_changed()";

    int index = get_slot_by_inv (inv);
    if (index == -1)
    {
        PJ_LOG (5, (THIS_FUNCTION, "error getting slot index. Function halted"));
        return;
    }
    
    slot_t *slot = &slots[index]; // through inv->mod_data
    if (!slot->busy)
        return;

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) 
    {
	    PJ_LOG(3,(THIS_FUNCTION, "Call DISCONNECTED [reason=%d (%s)]", 
		  inv->cause,
		  pjsip_get_status_text(inv->cause)->ptr));

	    PJ_LOG(3,(THIS_FUNCTION, "One call completed, wait next one..."));

        free_slot_by_inv (inv);

    } 
    else 
    {
	    PJ_LOG(3,(THIS_FUNCTION, "\nCall state changed to %s\n", 
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


static pj_bool_t on_rx_request (pjsip_rx_data *rdata) 
{
    const char THIS_FUNCTION[] = "on_rx_request()";
    pj_sockaddr hostaddr;
    
    char temp[80], hostip[PJ_INET6_ADDRSTRLEN];
    pj_str_t local_uri;
    pjsip_dialog *dlg;
    pjmedia_sdp_session *local_sdp;
    pjsip_inv_session *inv;
    pjsip_tx_data *tdata=NULL;
    
    //printf ("\n\n%s\n\n", rdata->pkt_info.src_name);
    //printf ("%s\n", rdata->msg_info.to->uri);

    unsigned options = 0;
    pj_status_t status;
    
    // get free slot

    
    
    
    // Respond (statelessly) any non-INVITE requests with 500 
     
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_INVITE_METHOD) 
    {
	    if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) 
        {
	        pj_str_t reason = pj_str("TASK 3.3 unable to handle "
				     "this request");

	        status = pjsip_endpt_respond_stateless( sip_endpt, rdata, 
					   500, &reason,
					   NULL, NULL);
            if (PJ_SUCCESS != status)
                pj_perror (5, THIS_FUNCTION, status, "pjsip_endpt_respond_stateless()");
	    }
	    return PJ_TRUE;
    }


    

    
    slot_t *tmp = get_slot ();
    
    if (tmp == NULL) 
    {
	    pj_str_t reason = pj_str("Busy here");

	    status = pjsip_endpt_respond_stateless( sip_endpt, rdata, 
				       486, &reason,
				       NULL, NULL);
        if (PJ_SUCCESS != status)
            pj_perror (5, THIS_FUNCTION, status, "pjsip_endpt_respond_stateless()");
        else 
            PJ_LOG (5, (THIS_FUNCTION, "get_slot() returned NULL. Responded 486 Busy Here"));

	    return PJ_TRUE;

    }
    
    PJ_LOG (5, (THIS_FUNCTION, "called for slot #%d", tmp->index));
    

   // Verify that we can handle the request. 
    status = pjsip_inv_verify_request(rdata, &options, NULL, NULL,
				      sip_endpt, NULL);
    if (status != PJ_SUCCESS) 
    {
	    pj_str_t reason = pj_str("Sorry Simple UA can not handle this INVITE");

	    status = pjsip_endpt_respond_stateless( sip_endpt, rdata, 
				       500, &reason,
				       NULL, NULL);
        if (PJ_SUCCESS != status)
            pj_perror (5, THIS_FUNCTION, status, "pjsip_endpt_respond_stateless()");
        else
            PJ_LOG (5, (THIS_FUNCTION, "pjsip_inv_verify_request() unsuccessfull. Responded 500 Internal Server Error"));
	    return PJ_TRUE;
    } 

    
     //Generate Contact URI
     
    if (pj_gethostip(pj_AF_INET(), &hostaddr) != PJ_SUCCESS) 
    {
	    pj_perror (5, THIS_FUNCTION, status, "Unable to retrieve local host IP");
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
    if (status != PJ_SUCCESS) 
    {
        pj_perror (5, THIS_FUNCTION, status, "pjsip_dlg_create_uas_and_inc_lock()");
	    status = pjsip_endpt_respond_stateless(sip_endpt, rdata, 500, NULL,
				      NULL, NULL);
        if (PJ_SUCCESS != status)
            pj_perror (5, THIS_FUNCTION, status, "pjsip_endpt_respond_stateless()");
        else
            PJ_LOG (5, (THIS_FUNCTION, "pjsip_dlg_create_uas_and_inc_lock() unsuccessfull. Responded 500 Internal Server Error")); 
	    return PJ_TRUE;
    }

    

     // Get media capability from media endpoint: 
    
    
    
    pjmedia_sock_info   media_sock_info;
    pjmedia_transport *media_transport; 
    
    status = pjmedia_transport_udp_create3(media_endpt, pj_AF_INET(), NULL, NULL, 
					       RTP_PORT + timer_count, 0, 
					       &media_transport);
	if (status != PJ_SUCCESS) 
    {
	    pj_perror (5, THIS_FUNCTION, status, "pjmedia_transport_udp_create3()");
	    halt ();
	}

    pjmedia_transport_info mti;
	pjmedia_transport_info_init(&mti);
	status = pjmedia_transport_get_info(media_transport, &mti);
    if (PJ_SUCCESS != status)
    {
        pj_perror (5, THIS_FUNCTION, status, "pjmedia_transport_get_info()");
    }

	pj_memcpy(&media_sock_info, &mti.sock_info,
		  sizeof(pjmedia_sock_info));
    
    status = pjmedia_endpt_create_sdp(media_endpt, dlg->pool,
				       1, &media_sock_info, &local_sdp);
    if (status != PJ_SUCCESS) 
    {
	    pj_perror (5, THIS_FUNCTION, status, "pjmedia_endpt_create_sdp()");
        pjsip_dlg_dec_lock(dlg);
	    return PJ_TRUE;
    }


    /* 
     * Create invite session, and pass both the UAS dialog and the SDP
     * capability to the session.
     */
    status = pjsip_inv_create_uas( dlg, rdata, local_sdp, 0, &inv);
    if (status != PJ_SUCCESS) 
    {
	    pj_perror (5, THIS_FUNCTION, status, "pjsip_inv_create_uas()");
        pjsip_dlg_dec_lock(dlg);
	    return PJ_TRUE;
    }

    inv->mod_data[mod_simpleua.id] = (void*)&tmp->index;
    
    
    
    pjsip_sip_uri *sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(rdata->msg_info.to->uri);
    
    char telephone[64] =  {0};

    strncpy (telephone, sip_uri->user.ptr, sip_uri->user.slen);
    PJ_LOG (5, (THIS_FUNCTION, "Got tel. number: %s\n", telephone));
    strncpy (tmp->uri, sip_uri->user.ptr, sip_uri->user.slen);
    

    // Register slot

    tmp->inv_ss = inv;
    tmp->dlg = dlg;
    tmp->rdata = rdata;
    tmp->tdata = tdata;
    tmp->ss_pool = tmp->inv_ss->pool; 
    tmp->media_sock_info = &media_sock_info;
    tmp->media_transport = media_transport;

    strncpy (tmp->telephone, telephone, sizeof(telephone));

    pj_timer_entry_init (&tmp->entry[0], tmp->entry_id[0] =  timer_count++, (void*)tmp, &accept_call);
    pj_timer_entry_init (&tmp->entry[1], tmp->entry_id[0] =  timer_count++, (void*)tmp, &auto_exit);

    
    /*
     * Initially send 100 response.
     *
     * The very first response to an INVITE must be created with
     * pjsip_inv_initial_answer(). Subsequent responses to the same
     * transaction MUST use pjsip_inv_answer().
     */

    status = pjsip_inv_initial_answer(tmp->inv_ss, rdata, 
				      100, 
				      NULL, NULL, &tdata);
    if (PJ_SUCCESS != status)
    {
        pj_perror (5, THIS_FUNCTION, status, "pjsip_inv_initial_answer() in slot #%d", tmp->index);
        emergency_exit ();
    }
   
    status = pjsip_inv_send_msg(tmp->inv_ss, tdata); 
    if (PJ_SUCCESS != status)
    {
        pj_perror (5, THIS_FUNCTION, status, "pjsip_inv_send_msg()");
        emergency_exit ();
    }

    if ( !strcmp (telephone, "666") )
        tmp->input_port = warning_conf_id;
    
    else if ( !strcmp (telephone, "1234") )
        tmp->input_port = quick_beep_conf_id;

    else if ( !strcmp (telephone, "9000") )
        tmp->input_port = station_answer_conf_id;
    else if ( !strcmp (telephone, "05") )
        tmp->input_port = player_conf_id;
    else
    {
        status = pjsip_inv_answer
                (
                    tmp->inv_ss,
                    404,                
                    NULL, NULL, &tdata
                );
    
        if (PJ_SUCCESS != status)
        {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_inv_answer()");
            emergency_exit ();
        }

        status = pjsip_inv_send_msg(tmp->inv_ss, tdata);

        if (PJ_SUCCESS != status)
        {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_inv_send_msg()");
            emergency_exit ();
        }
        free_slot (tmp);
        return PJ_TRUE;
    }

    status = pjsip_inv_answer
                (
                    tmp->inv_ss,
                    183,                
                    NULL, NULL, &tdata
                );
    
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_inv_answer()");
            emergency_exit ();
    }
    
    status = pjsip_inv_send_msg(tmp->inv_ss, tdata);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_inv_send_msg()");
            emergency_exit ();
    } 

    return PJ_TRUE;
}



/*
 * Callback when SDP negotiation has completed.
 * We are interested with this callback because we want to start media
 * as soon as SDP negotiation is completed.
 */

// Also register slots: master port, stream port, stream
// Starts: master port, timer for exit
static void call_on_media_update (pjsip_inv_session *inv, pj_status_t status)
{
    const char THIS_FUNCTION[] = "call_on_media_update()";
    pjmedia_stream_info stream_info;
    const pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *remote_sdp;

    

    int index = get_slot_by_inv (inv);
    if (index == -1)
    {
        PJ_LOG (5, (THIS_FUNCTION, "error in get_slot_by_inv"));
        emergency_exit ();
    }
    
    PJ_LOG (5, (THIS_FUNCTION, "called for slot #%d", index));
    
    slot_t *tmp = &slots[index];

    if (status != PJ_SUCCESS) 
    {
        pj_perror (5, THIS_FUNCTION, status, "argument 2 (status) invalid");
        emergency_exit ();
    }
    

    /* Get local and remote SDP.
     * We need both SDPs to create a media session.
     */
    status = pjmedia_sdp_neg_get_active_local(tmp->inv_ss->neg, &local_sdp);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_sdp_neg_get_active_local()");
            emergency_exit ();
    } 

    status = pjmedia_sdp_neg_get_active_remote(tmp->inv_ss->neg, &remote_sdp);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_sdp_neg_get_active_remote()");
            emergency_exit ();
    } 

    // Create stream info based on the media audio SDP. 
    status = pjmedia_stream_info_from_sdp(&stream_info, tmp->ss_pool,
					  media_endpt,
					  local_sdp, remote_sdp, 0);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_stream_info_from_sdp()");
            emergency_exit ();
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
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_stream_create()");
            emergency_exit ();
    } 

    // Start the audio stream 
    status = pjmedia_stream_start(tmp->media_stream);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_stream_start()");
            emergency_exit ();
    } 
	
    // Start the UDP media transport 
    status = pjmedia_transport_media_start(tmp->media_transport, 0, 0, 0, 0);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_transport_media_start()");
            emergency_exit ();
    } 
    
    status = pjmedia_stream_get_port(tmp->media_stream, &tmp->stream_port);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_stream_get_port()");
            emergency_exit ();
    }

    pj_str_t *conf_port_name = pj_pool_alloc (tmp->ss_pool, 8);
    if (conf_port_name == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"pj_pool_alloc() for pj_str_t *conf_port_name failed"));
        emergency_exit ();
    }

    char tmps[8];
    sprintf (tmps, "%d", index);
    *conf_port_name = pj_str (tmps);

    status = pjmedia_conf_add_port (conf, tmp->ss_pool, tmp->stream_port, conf_port_name, &tmp->conf_id);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_add_port()");
            emergency_exit ();
    }
     

	
    
    status = pjmedia_conf_connect_port (conf, ringback_conf_id, tmp->conf_id, 128);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_connect_port()");
            emergency_exit ();
    }
    

    
    status = pj_timer_heap_schedule(timer_heap, &tmp->entry[0], &delay1); // start timer to accept
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pj_timer_heap_schedule()");
            emergency_exit ();
    }
    PJ_LOG (5, (THIS_FUNCTION, "exited for slot #%d", index));
    


    // Done with media. 
}



/////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
    const char THIS_FUNCTION[] = "main()";
    printf ("\n");
    PJ_LOG (5, (THIS_FUNCTION, "Application started\n\n"));

    if (argc > 1)
    {
        if (argc < 3)
        {
            printf ("Too few arguments for app\n");
            printf ("USAGE: %s <SIP port> <RTP port>\n", argv[0]);
            exit (1);
        }
        while (1)
        {
            int option_symbol;
            int option_index;
            static struct option long_options[] = 
            {
                {"sip-port", 1, 0, 's'},
                {"rtp-port", 1, 0, 'r'},
                {0, 0, 0, 0}
            };
            option_symbol = getopt_long_only (argc, argv, "sip-port:rtp-port:", long_options, &option_index);
            switch (option_symbol)
            {
                case 's':
                    if (optarg)
                    {
                        SIP_PORT = atoi (optarg);
                        printf ("Set SIP port to %s\n", optarg);
                    }
                    break;

                case 'r':
                    if (optarg)
                    {
                        RTP_PORT = atoi (optarg);
                        printf ("Set RTP port to %s\n", optarg);
                    }
            }
            if (option_symbol == -1)
                break;
        }
        if (SIP_PORT < 4000 || RTP_PORT < 4000)
            {
                printf ("Ports numbers must be more than 4000\n");
                exit (1);
            }

    }    

    pj_status_t status;

    status = pj_init();
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pj_init()");
            halt ();
    }

    pj_log_set_level(5);

    status = pjlib_util_init();
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjlib_util_init()");
            halt ();
    }

    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

	pool = pj_pool_create( &cp.factory,	    // pool factory	    
			   "global pool",	    // pool name.	    
			   4000,	    // init size	    
			   4000,	    // increment size	    
			   &poolfail_cb		    // callback on error    
			   );
    // Init slots
    for (pj_uint8_t i2=0; i2<20; i2++)
    {
        nullize_slot (&slots[i2]);
        status = pj_mutex_create (pool, "global slots_count mutex", PJ_MUTEX_SIMPLE, &slots[i2].mutex);
        if (PJ_SUCCESS != status)
        {
            pj_perror (5, THIS_FUNCTION, status, "pj_mutex_create()");
            halt ();
        }
        slots[i2].index = i2;
    }

    
    // Must create a pool factory before we can allocate any memory. 
    
    // Create global endpoint: 
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
	if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_endpt_create()");
            halt ();
    }

    

    /* 
     * Add UDP transport, with hard-coded port 
     * Alternatively, application can use pjsip_udp_transport_attach() to
     * start UDP transport, if it already has an UDP socket (e.g. after it
     * resolves the address with STUN).
     */
    
	pj_sockaddr addr;


	
    status = pj_sockaddr_init (pj_AF_INET(), &addr, NULL, (pj_uint16_t)SIP_PORT);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pj_sockaddr_init()");
            halt ();
    }
	
	
	status = pjsip_udp_transport_start (sip_endpt, &addr.ipv4, NULL, 1, NULL);
	if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_udp_transport_start()");
            halt ();
    }


    /* 
     * Init transaction layer.
     * This will create/initialize transaction hash tables etc.
     */
    status = pjsip_tsx_layer_init_module (sip_endpt);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_tsx_layer_init_module()");
            halt ();
    }


    /* 
     * Initialize UA layer module.
     * This will create/initialize dialog hash tables etc.
     */
    status = pjsip_ua_init_module (sip_endpt, NULL );
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_ua_init_module()");
            emergency_exit ();
    }

   
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
	pj_bzero (&inv_cb, sizeof(inv_cb));
	inv_cb.on_state_changed = &call_on_state_changed;
	inv_cb.on_new_session = &call_on_forked;
	inv_cb.on_media_update = &call_on_media_update;

	// Initialize invite session module:  
	status = pjsip_inv_usage_init (sip_endpt, &inv_cb);
	if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_inv_usage_init()");
            emergency_exit ();
    }
    }

    // Initialize 100rel support 
    status = pjsip_100rel_init_module (sip_endpt);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_100rel_init_module()");
            emergency_exit ();
    }

    
     // Register our module to receive incoming requests.
     
    status = pjsip_endpt_register_module (sip_endpt, &mod_simpleua);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_endpt_register_module() for mod_simpleua");
            emergency_exit ();
    }

    
     // Register message logger module.
     
    status = pjsip_endpt_register_module (sip_endpt, &msg_logger);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_endpt_register_module() for msg_logger");
            emergency_exit ();
    }
    
    timer_heap = pjsip_endpt_get_timer_heap (sip_endpt); 
    /* 
     * Initialize media endpoint.
     * This will implicitly initialize PJMEDIA too.
     */
 
    status = pjmedia_endpt_create (&cp.factory, NULL, 1, &media_endpt);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_endpt_create()");
            emergency_exit ();
    }


    status = pjmedia_codec_g711_init (media_endpt);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_codec_g711_init()");
            emergency_exit ();
    }

    // Create event manager 
    status = pjmedia_event_mgr_create (pool, 0, NULL);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_event_mgr_create()");
            emergency_exit ();
    }
    
    
    /* 
     * Create media transport used to send/receive RTP/RTCP socket.
     * One media transport is needed for each call. Application may
     * opt to re-use the same media transport for subsequent calls.
     */
    

    
    pjmedia_tonegen_create (pool, 8000, 1, 160, 16, 0, &ringback_tonegen_port);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_tonegen_create() for ringback_tonegen_port");
            emergency_exit ();
    }
    
    pjmedia_tonegen_create (pool, 8000, 1, 160, 16, 0, &station_answer_tonegen_port);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_tonegen_create() for station_answer_tonegen_port");
            emergency_exit ();
    }
    
    pjmedia_tonegen_create (pool, 8000, 1, 160, 16, 0, &quick_beep_tonegen_port);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_tonegen_create() for quick_beep_tonegen_port");
            emergency_exit ();
    }
    
    pjmedia_tonegen_create (pool, 8000, 1, 160, 16, 0, &warning_tonegen_port);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_tonegen_create() for warning_tonegen_port");
            emergency_exit ();
    }

    pjmedia_tone_desc ringback_tone, warning_tone, quick_beep_tone, station_answer_tone;

    ringback_tone.freq1 = 425;
    ringback_tone.freq2 = 0;
    ringback_tone.on_msec = 1000;
    ringback_tone.off_msec = 4000; // set to 0 if need station ready sound

    station_answer_tone.freq1 = 425;
    station_answer_tone.freq2 = 0;
    station_answer_tone.on_msec = 1;
    station_answer_tone.off_msec = 0;

    warning_tone.freq1 = 200;
    warning_tone.freq2 = 0;
    warning_tone.on_msec = 200;
    warning_tone.off_msec = 200;

    quick_beep_tone.freq1 = 500;
    quick_beep_tone.freq2 = 0;
    quick_beep_tone.on_msec = 70;
    quick_beep_tone.off_msec = 70;


    pjmedia_tonegen_play (ringback_tonegen_port, 1, &ringback_tone, PJMEDIA_TONEGEN_LOOP);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_tonegen_play() for ringback_tonegen_port");
            emergency_exit ();
    }
    
    pjmedia_tonegen_play (quick_beep_tonegen_port, 1, &quick_beep_tone, PJMEDIA_TONEGEN_LOOP);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_tonegen_play() for quick_beep_tonegen_port");
            emergency_exit ();
    }
    
    pjmedia_tonegen_play (warning_tonegen_port, 1, &warning_tone, PJMEDIA_TONEGEN_LOOP);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_tonegen_play() for warning_tonegen_port");
            emergency_exit ();
    }
    
    pjmedia_tonegen_play (station_answer_tonegen_port, 1, &station_answer_tone, PJMEDIA_TONEGEN_LOOP);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_tonegen_play() for station_answer_tonegen_port");
            emergency_exit ();
    }
   

    status = pjmedia_wav_player_port_create 
                    (  
                        pool,	// memory pool	    
					    "task3.wav",	// file to play	 (16 bitrate, 8 khz, mono)
					    20,	// ptime.	    
					    0,	// flags	    
					    0,	// default buffer   
					    &player_port// returned port    
				    );
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_wav_player_port_create()");
            emergency_exit ();
    }
    
    status = pjmedia_conf_create (pool, 26, 8000, 1, 160, 16, PJMEDIA_CONF_NO_DEVICE, &conf);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_create()");
            emergency_exit ();
    }
        
    pj_str_t mp_name = pj_str ("conf mp name");
    status = pjmedia_conf_set_port0_name (conf, &mp_name);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_set_port0_name()");
            emergency_exit ();
    }

    pjmedia_port *null_port;
    status = pjmedia_null_port_create (pool, 8000, 1, 160, 16, &null_port);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_null_port_create()");
            emergency_exit ();
    }

    pjmedia_port *conf_p0 = pjmedia_conf_get_master_port(conf); 

    status = pjmedia_master_port_create (pool, null_port, conf_p0, 0, &conf_mp);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_master_port_create()");
            emergency_exit ();
    }

    status = pjmedia_master_port_start (conf_mp);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_master_port_start()");
            emergency_exit ();
    }


    pj_str_t conf_tp_names[5];
    conf_tp_names[0] = pj_str ("warning tp");
    conf_tp_names[1] = pj_str ("sa tp");
    conf_tp_names[2] = pj_str ("rb tp");
    conf_tp_names[3] = pj_str ("qb tp");
    conf_tp_names[4] = pj_str ("player p");
    
    status = pjmedia_conf_add_port (conf, pool, warning_tonegen_port, &conf_tp_names[0], &warning_conf_id);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_add_port() for warning_tonegen_port");
            emergency_exit ();
    }
        
    status = pjmedia_conf_add_port (conf, pool, station_answer_tonegen_port, &conf_tp_names[1] , &station_answer_conf_id);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_add_port() for station_answer_tonegen_port");
            emergency_exit ();
    }
    
    status = pjmedia_conf_add_port (conf, pool, ringback_tonegen_port, &conf_tp_names[2], &ringback_conf_id);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_add_port() for ringback_tonegen_port");
            emergency_exit ();
    }
    
    status = pjmedia_conf_add_port (conf, pool, quick_beep_tonegen_port, &conf_tp_names[3], &quick_beep_conf_id);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_add_port() for quick_beep_tonegen_port");
            emergency_exit ();
    }
    
    status = pjmedia_conf_add_port (conf, pool, player_port, &conf_tp_names[4], &player_conf_id);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_add_port() for player_port");
            emergency_exit ();
    }

    printf ("\n\n>> READY TO ACCEPT INCOMING CALLS... <<\n\n");

    pj_thread_t *handles_thread=NULL;
    pj_thread_desc thread_desc; 
    status = pj_thread_register ("timer thread", thread_desc, &handles_thread);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pj_thread_register()");
            emergency_exit ();
    }

    status = pj_thread_create (pool, "timer thread", (pj_thread_proc*)thread_func, NULL, PJ_THREAD_DEFAULT_STACK_SIZE, 0, &handles_thread);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pj_thread_create()");
            emergency_exit ();
    }
    
/////////////////////////////////////////////////////////////////////////////
    
    pj_time_val timeout = {0, 10};
	
    while (g_complete)
    {
        status = pjsip_endpt_handle_events(sip_endpt, &timeout);
        if (PJ_SUCCESS != status)
        {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_endpt_handle_events()");
            emergency_exit ();
        }

        if (pause1 || to_exit || to_halt)
        {
            if (to_halt)
                halt ();
            if (to_exit)
                emergency_exit ();
            printf ("\n\nPRESS 'P' TO CONTINUE...\n\n");
            while (pause1)
                sleep (1);
                
        }
        
    } 
/////////////////////////////////////////////////////////////////////////////

    status = pj_thread_destroy (handles_thread);
    if (PJ_SUCCESS != status)
    {
        pj_perror (5, THIS_FUNCTION, status, "pj_thread_destroy()");
        emergency_exit ();
    }

    for (int i=0; i<20; i++)
        destroy_slot (&slots[i]);

    if (conf_mp)
        pjmedia_master_port_destroy (conf_mp, PJ_FALSE);

    status = pjmedia_conf_destroy (conf);
    if (PJ_SUCCESS != status)
    {
        pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_destroy()");
        emergency_exit ();
    }

    
    
    

    if (sip_endpt)
	    pjsip_endpt_destroy(sip_endpt); 

    // Release pool 
    if (pool)
	    pj_pool_release(pool);
    pj_caching_pool_destroy (&cp); 
    printf ("\n\n");
    PJ_LOG (5, (THIS_FUNCTION, "Application finished normally\n"));
    return 0;
}

/////////Custom and unused////////////////////////////

/////////Custom funcs/////////////////////////////

void free_all_slots ()
{
    for (int i=0; i<20; i++)
        free_slot (&slots[i]);
}

void emergency_exit ()
{
    printf ("\n\nEmergency exit called\n\n");
    for (int i=0; i<20; i++)
        destroy_slot (&slots[i]);
    
    if (conf)
        pjmedia_conf_destroy (conf);
    
    if (pool)
	    pj_pool_release(pool);
    if (sip_endpt)
	    pjsip_endpt_destroy(sip_endpt);

    pj_caching_pool_destroy (&cp);

    exit (3);
}

void halt ()
{
    printf ("\n\nHalt called\n\n");
    if (pool)
	    pj_pool_release(pool);
    if (sip_endpt)
	    pjsip_endpt_destroy(sip_endpt);
    exit (2);
}

void destroy_slot (slot_t *slot)
{
    const char THIS_FUNCTION[] = "destroy_slot()";
    free_slot (slot); 
    pj_status_t status = pj_mutex_destroy (slot->mutex);
    if (PJ_SUCCESS != status)
    {
        pj_perror (5, THIS_FUNCTION, status, "pj_mutex_destroy() for slot #%d", slot->index);
        halt ();
    }
}

int thread_func (void *p)
{
    int id;
    while (1)
    {
       
        switch ( getchar() )
        {
            case 'h':
                to_halt = PJ_TRUE;
                return 0;
            case 'e':
                to_exit = PJ_TRUE;
                return 0;
            case 'q':
                g_complete = PJ_FALSE;
                return 0;
            
            case 'c':
                free_all_slots ();
                break;
            
            case 's':
                printf ("\n\n\tCOUNT_SLOTS = %u\n\n", slots_count);
                break;

            case 'p':
                pause1 = !pause1;
                break;

            case 't':
                for (int i=0; i<20; i++)
                {
                    if (slots[i].busy)
                    {
                        printf ("\n\n%s (%s)\n\n", slots[i].telephone, slots[i].uri);
                    } 
                        
                }
                break;
            
            case 'b':
                printf ("\n\nEnter slot ID: ");
                scanf ("%d", &id);
                printf ("\n\nSTREAM CONF ID %u\n\n", slots[id].conf_id);
                break;
        }
            
    }
    
    return 0;
}



slot_t * get_slot ()
{
    const char THIS_FUNCTION[] = "get_slot()";
    pj_status_t status;
    for (int i=0; i<20; i++)
        if (!slots[i].busy)
            if ((status = pj_mutex_trylock (slots[i].mutex)) == PJ_SUCCESS)
            {
                PJ_LOG (5, (THIS_FUNCTION, "got free mutex in slot #%d", i));
                slots[i].busy = PJ_TRUE;
                slots_count--;
                pj_mutex_unlock (slots[i].mutex);
                if (PJ_SUCCESS != status)
                {
                    pj_perror (5, THIS_FUNCTION, status, "pj_mutex_trylock() for slot #%d", i);
                    emergency_exit ();
                }
                return &slots[i];
            }
            /*else
            {
                if (PJ_SUCCESS != status)
                {
                    pj_perror (5, THIS_FUNCTION, status, "pj_mutex_trylock() for slot #%d", i);
                    emergency_exit ();
                }
            } */
    return NULL;
}

void nullize_slot (slot_t *slot)
{
    slot->media_transport = NULL;
    slot->media_stream = NULL;
    slot->stream_port = NULL;
    slot->mp = NULL;
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
    slot->conf_id = 21;
    slot->input_port = 21;
}

int get_slot_by_inv (pjsip_inv_session *inv) // zdes byla baga sho DESTROYING SLOT #0 (mb ostal'nye ne free); ksta po etoi je prichine mb sipp lagal
{
    const char THIS_FUNCTION[] = "get_slot_by_inv()";
    if (inv != NULL)
        return (int) *(int*)inv->mod_data[mod_simpleua.id];
    PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR "inv==NULL"));
    return -1;
}

void free_slot (slot_t *slot)
{
    const char THIS_FUNCTION[] = "free_slot()";
    pj_status_t status;
    
    if (slot == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"slot==NULL (slot pointer is empty)"));
    }
        
    if (!slot->busy)
        return;

    if ( (status = pj_mutex_trylock(slot->mutex)) != PJ_SUCCESS )
    {
        pj_perror (5, THIS_FUNCTION, status, "pj_mutex_trylock()");
        return;
    }
        
    
    PJ_LOG (5, (THIS_FUNCTION, "destroying slot #%d", slot->index));
    

    if (slot->conf_id < 21 && slot->input_port < 21)
    {
        status = pjmedia_conf_disconnect_port (conf, slot->input_port, slot->conf_id);
        if (PJ_SUCCESS != status)
        {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_disconnect_port() for slot #%d", slot->index);
            emergency_exit ();
        }
        status = pjmedia_conf_remove_port (conf, slot->conf_id);
        if (PJ_SUCCESS != status)
        {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_remove_port() for slot #%d", slot->index);
            emergency_exit ();
        }
    }        
    
    

    if (slot->inv_ss->state == PJSIP_INV_STATE_CONFIRMED)
    {
            pjsip_tx_data *request_data=NULL;
            
            pjsip_dlg_create_request (slot->dlg, &pjsip_bye_method, -1, &request_data);
            if (PJ_SUCCESS != status)
            {
                pj_perror (5, THIS_FUNCTION, status, "pjsip_dlg_create_request() for slot #%d", slot->index);
                emergency_exit ();
            }

            pjsip_dlg_send_request (slot->dlg, request_data, -1, NULL);
            if (PJ_SUCCESS != status)
            {
                pj_perror (5, THIS_FUNCTION, status, "pjsip_dlg_send_request() for slot #%d", slot->index);
                emergency_exit ();
            }
    } 

    else if (slot->inv_ss->state == PJSIP_INV_STATE_EARLY)
    {
            pjsip_inv_answer
                (
                   slot->inv_ss, 486, NULL, NULL, &slot->tdata 
                );
            if (PJ_SUCCESS != status)
            {
                pj_perror (5, THIS_FUNCTION, status, "pjsip_inv_answer() for slot #%d", slot->index);
                emergency_exit ();
            }
            
            pjsip_inv_send_msg(slot->inv_ss, slot->tdata);
            if (PJ_SUCCESS != status)
            {
                pj_perror (5, THIS_FUNCTION, status, "pjsip_inv_send_msg() for slot #%d", slot->index);
                emergency_exit ();
            }
    }

    if (slot->media_stream)
    {
        pjmedia_transport_media_stop (slot->media_transport);
        pjmedia_stream_destroy (slot->media_stream);
        if (PJ_SUCCESS != status)
        {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_stream_destroy() for slot #%d", slot->index);
            emergency_exit ();
        }
    }
        
    if (slot->media_transport)
    {
        pjmedia_transport_close (slot->media_transport); 
        if (PJ_SUCCESS != status)
        {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_transport_close() for slot #%d", slot->index);
            emergency_exit ();
        }
    }
        
    pj_timer_heap_cancel_if_active (timer_heap, &slot->entry[0], slot->entry_id[0]);
    pj_timer_heap_cancel_if_active (timer_heap, &slot->entry[1], slot->entry_id[1]);
 
    if (slot->inv_ss->state != PJSIP_INV_STATE_DISCONNECTED)
    {
        status = pjsip_inv_end_session (slot->inv_ss, 200, NULL, &slot->tdata);
        if (PJ_SUCCESS != status)
        {
            pj_perror (5, THIS_FUNCTION, status, "pjsip_inv_end_session() for slot #%d", slot->index);
            emergency_exit ();
        }
        pjsip_inv_send_msg(slot->inv_ss, slot->tdata);
    }
    

    nullize_slot (slot);
    
    slots_count++;
    
    slot->busy = PJ_FALSE;

    pj_mutex_unlock (slot->mutex);
    if (PJ_SUCCESS != status)
    {
        pj_perror (5, THIS_FUNCTION, status, "pj_mutex_unlock() for slot #%d", slot->index);
        emergency_exit ();
    }
    PJ_LOG (5, (THIS_FUNCTION, "freied slot %d", slot->index));
}

void free_slot_by_inv (pjsip_inv_session *inv)
{
    const char THIS_FUNCTION[] = "free_slot_by_inv()";
    int index = get_slot_by_inv (inv);
    if (index == -1)
    {
        PJ_LOG (5, (THIS_FUNCTION, "slot already clear or an error"));
        return;
    }
    free_slot (&slots[index]);
}


void accept_call(pj_timer_heap_t *ht, pj_timer_entry *e) // callback
{
    const char THIS_FUNCTION[] = "accept_call()";
    
    pj_status_t status;
    
    PJ_UNUSED_ARG(ht);
    
    slot_t *tmp = (slot_t*)e->user_data;
    if (tmp == NULL)
    {
        PJ_LOG ( 5, (THIS_FUNCTION, PJ_LOG_ERROR"(slot_t*)e->user_data returned NULL") );
        return;
    }

    int index = tmp->index;
    if (!tmp->busy)
    {
        PJ_LOG(5, (THIS_FUNCTION, PJ_LOG_ERROR"slot #%d isn't busy", index));
    }
    
    if (tmp->input_port == 21)
    {
        PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"tmp->input_port is not set"));
        free_slot_by_inv (tmp->inv_ss);
        return;
    } 

    //pjmedia_master_port_stop (conf_mp);

    status = pjmedia_conf_disconnect_port (conf, ringback_conf_id, tmp->conf_id);
    if (PJ_SUCCESS != status)
    {
        pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_disconnect_port for slot #%d", index);
        emergency_exit ();
    }

    status = pjmedia_conf_connect_port (conf, tmp->input_port, tmp->conf_id, 0);
    if (PJ_SUCCESS != status)
    {
        pj_perror (5, THIS_FUNCTION, status, "pjmedia_conf_connect_port for slot #%d", index);
        emergency_exit ();
    }

    //pjmedia_master_port_start (conf_mp);

    status =   pjsip_inv_answer (tmp->inv_ss, 200, NULL, NULL, &tmp->tdata);
    if (PJ_SUCCESS != status)
    {
        pj_perror (5, THIS_FUNCTION, status, "pjsip_inv_answer() for slot #%d", index);
        emergency_exit ();
    }

    status = pjsip_inv_send_msg(tmp->inv_ss, tmp->tdata);
    if (PJ_SUCCESS != status)
    {
        pj_perror (5, THIS_FUNCTION, status, "pjsip_inv_send_msg() for slot #%d", index);
        emergency_exit ();
    }
   
    status = pj_timer_heap_schedule(timer_heap, &tmp->entry[1], &delay2);
    if (PJ_SUCCESS != status)
    {
        pj_perror (5, THIS_FUNCTION, status, "pj_timer_heap_schedule() for slot #%d", index);
        emergency_exit ();
    }
}
 
void auto_exit (pj_timer_heap_t *ht, pj_timer_entry *e) // callback
{
    PJ_UNUSED_ARG(ht);
    const char THIS_FUNCTION[] = "auto_exit()";
    slot_t *tmp = (slot_t*)e->user_data;

    if (tmp == NULL)
    {
        PJ_LOG ( 5, (THIS_FUNCTION, PJ_LOG_ERROR"(slot_t*)e->user_data returned NULL") );
        return;
    }

    if (tmp->busy)
        free_slot_by_inv (tmp->inv_ss); 

}

// prosto tak dobavil
void poolfail_cb (pj_pool_t *pool, pj_size_t size)
{
    const char THIS_FUNCTION[] = "poolfail_cb()";
    PJ_LOG ( 5, (THIS_FUNCTION, PJ_LOG_ERROR"Unknown fatal error") );
    halt ();
}

//////////////////////////////////////////////////////////////////////////////////

// Notification on incoming messages 
static pj_bool_t logging_on_rx_msg(pjsip_rx_data *rdata)
{
    PJ_LOG(4,(APPNAME, "RX %d bytes %s from %s %s:%d:\n"
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

    PJ_LOG(4,(APPNAME, "TX %d bytes %s to %s %s:%d:\n"
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
    const char THIS_FUNCTION[] = "call_on_forked()";
    PJ_LOG (3, (APPNAME, "\n\n!!!DIALOG FORKED!!!\n\n"));
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);
}
