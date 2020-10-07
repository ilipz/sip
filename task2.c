#include <pjsua-lib/pjsua.h>
#include <unistd.h>

#define THIS_FILE	"APP"

#define SIP_DOMAIN	"example.com"
#define SIP_USER	"alice"
#define SIP_PASSWD	"secret"


int done = 1;
pjsua_call_id callid;

pjmedia_port *tone_port;

pjmedia_tone_desc tone[1];

pj_pool_t *pool;
pj_caching_pool cp;

static void on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info ci;

    pjsua_call_get_info(call_id, &ci);

    if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
	
        pjmedia_tonegen_play(tone_port, 1, tone, 0);
        pjsua_conf_add_port (pool, tone_port, NULL);

    }
}


/* Callback called by the library upon receiving incoming call */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
			     pjsip_rx_data *rdata)
{
    pjsua_call_info ci;
    callid = call_id;
    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);

    pjsua_call_get_info(call_id, &ci);

    PJ_LOG(3,(THIS_FILE, "Incoming call from %.*s!!",
			 (int)ci.remote_info.slen,
			 ci.remote_info.ptr));

    /* Automatically answer incoming calls with 180OK */
    pjsua_call_answer(call_id, 180, NULL, NULL);
    pjsua_call_answer (call_id, 200, NULL, NULL);   
    
    sleep(30);
    pjsua_call_hangup_all();    
    
    done = 0;
}

/* Callback called by the library when call's state has changed */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info ci;

    PJ_UNUSED_ARG(e); 							// do not warn about unused arg - event *e

    pjsua_call_get_info(call_id, &ci);
    PJ_LOG(3,(THIS_FILE, "Call %d state=%.*s", call_id,
			 (int)ci.state_text.slen,
			 ci.state_text.ptr));
}

/* Display error and exit application */
static void error_exit(const char *title, pj_status_t status)
{
    pjsua_perror(THIS_FILE, title, status);
    pjsua_destroy();
    exit (1);
}

int main(int argc, char *argv[])
{
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "app",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );
    pjmedia_tonegen_create(pool, 8000, 1, 64, 16, 0, &tone_port);

    tone[0].freq1 = 200;
	tone[0].freq2 = 0;
	tone[0].on_msec = 100;
	tone[0].off_msec = 100;

    pjsua_acc_id acc_id;
    pj_status_t status;
    pj_log_set_decor (PJ_LOG_HAS_COLOR);
    /* Create pjsua first! */
    status = pjsua_create();
    if (status != PJ_SUCCESS) error_exit("Error in pjsua_create()", status);

    /* Init pjsua */
    {
	pjsua_config cfg;
	pjsua_logging_config log_cfg;

	pjsua_config_default(&cfg);
	cfg.cb.on_incoming_call = &on_incoming_call; 
	cfg.cb.on_call_state = &on_call_state;

	pjsua_logging_config_default(&log_cfg);
	log_cfg.console_level = 4;

    pjsua_media_config med_cfg;
    pjsua_media_config_default (&med_cfg);
	status = pjsua_init(&cfg, &log_cfg, &med_cfg);
	if (status != PJ_SUCCESS) error_exit("Error in pjsua_init()", status);
    }

    /* Add UDP transport. */
    {
	pjsua_transport_config cfg;

	pjsua_transport_config_default(&cfg);

	cfg.port = 5060;
	status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, NULL);
	if (status != PJ_SUCCESS) error_exit("Error creating transport", status);
    }

    /* Initialization is done, now start pjsua */
    status = pjsua_start();
    if (status != PJ_SUCCESS) error_exit("Error starting pjsua", status);

    /* Register to SIP server by creating SIP account. */
    {
	pjsua_acc_config cfg;
	
	pjsua_acc_config_default(&cfg);
	cfg.id = pj_str("sip:" SIP_USER "@" SIP_DOMAIN);
	cfg.reg_uri = pj_str("sip:" SIP_DOMAIN); 
	cfg.cred_count = 1;
	cfg.cred_info[0].realm = pj_str(SIP_DOMAIN); 
	cfg.cred_info[0].scheme = pj_str("digest"); 			// using hash sums
	cfg.cred_info[0].username = pj_str(SIP_USER);
	cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD; 	// authentication type - password
	cfg.cred_info[0].data = pj_str(SIP_PASSWD); 			// password

	status = pjsua_acc_add(&cfg, PJ_TRUE, &acc_id);
	if (status != PJ_SUCCESS) error_exit("Error adding account", status);
    } 

    while (done)
	    sleep (1);
    pjsua_call_hangup_all();
    pjsua_destroy();

    return 0;
}
