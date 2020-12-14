/*
    ROADMAP:
    1) Fixes: segfaults, catching bad PJ returned status, log 
    2) Configure (JSON, arg options)
    3) Different net interfaces
    -------- Optional
    + Ringback (KPV)
    + Direct RTP stream (faster and no codecs requires)
    + On/off leg media streams; record to .wav
    + Extended logging: log to file, logging from siprtp.c
    + Threads: free junctions in other thread
-----------------------------------------------------------------
- So problems with pjmedia_frames in transport. Should try close media transport after any call
*/
#include "types.h"
#include "cb/forked.h"
#include "cb/media_upd.h"
#include "cb/rx_req.h"
#include "cb/state_chd.h"
#include "juncs/free.h"
#include "loggers/rx_msg.h"
#include "loggers/siprtp.h"
#include "loggers/tx_msg.h"
#include "threads/console.h"
#include "threads/junc_controller.h"
#include "utils/exits.h"
#include "utils/inits.h"
#include "utils/util.h"

static char json_doc1[] =
"\"String\" : \"Stroka\"";


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
    {"05", "10.25.109.55:7060"},
    {"1vedro", "192.168.0.3:15060"},
    {"40002", "192.168.23.94:5070"},
    {"vedro", "192.168.41.250:15060"}
};

int main (int argc, char **argv)
{
    PJ_LOG (5, (APPNAME, "Application has been started"));
    printf ("\n\n\n");
    
    g.local_addr = pj_str(argv[1]);
    g.local_addr2 = pj_str(argv[2]);

    // TODO: set defaults
    // TODO: if arguments then parse them
    // TODO: else if file (json) then parse it

    
    static const char *THIS_FUNCTION = "main()";
    pj_status_t status;

    status = pj_init ();
    if (status != PJ_SUCCESS)
    {
        pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pj_init()");
        halt ("pj_init()");
    }

    status = pjlib_util_init();
    if (status != PJ_SUCCESS)
    {
        pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pjlib_util_init()");
        halt ("pjlib_util_init()");
    }

    pj_caching_pool_init(&g.cp, &pj_pool_factory_default_policy, 0);

    
    g.pool = pj_pool_create(&g.cp.factory, "app", 4000, 4000, NULL);    
    if (g.pool == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"global pool hasn't been created (pool pointer == NULL). App halted"));
        halt ("pj_pool_create()");
    }

    pj_pool_t *pool;
    pj_json_elem *elem;
    char *out_buf;
    unsigned size;
    pj_json_err_info err;

    pj_pool_factory mem;
    pool = g.pool;

    size = (unsigned)strlen(json_doc1);
    elem = pj_json_parse(pool, json_doc1, &size, &err);
    if (!elem) {
	PJ_LOG(1, (APPNAME, "  Error: json_verify_1() parse error"));
	;
    }

    pj_json_elem *head = elem;
    
    printf ("%s\n", head->value.str.ptr);

    return 0;
    size = (unsigned)strlen(json_doc1) * 2;
    out_buf = pj_pool_alloc(pool, size);

    if (pj_json_write(elem, out_buf, &size)) {
	PJ_LOG(1, (APPNAME, "  Error: json_verify_1() write error"));
	
    }

    PJ_LOG(3,(APPNAME, "Json document:\n%s", out_buf));
    pj_pool_release(pool);
    return 0;

    return 0; // temporary exit util pj_json tested
    init_exits ();

    g.to_quit = 0;
    g.pause = 0;

    // Create: conf bridge, nullport, conf master port, tonegen  

    // TODO: parse options and json
    
    init_sip ();    

    init_media ();

    init_juncs ();
    


    pj_thread_t *_console_thread;
    status = pj_thread_create (g.pool, "Console thread", console_thread, NULL, 0, 0, &_console_thread);
    if (status != PJ_SUCCESS)
        emergency_exit ("main()::pj_thread_create() for console thread", &status);

    pj_time_val timeout = {0, 10};
    
    while (!g.to_quit)
    {
        status = pjsip_endpt_handle_events (g.sip_endpt, &timeout);
        if (status != PJ_SUCCESS)
            emergency_exit ("main()::pjsip_endpt_handle_events()", &status);
        while (g.pause) sleep(1); 
    }

    for (int i=0; i<10; i++)
        free_junction (&g.junctions[i]);

    for (int i=0; i<10; i++)
        destroy_junction (&g.junctions[i]);
    
    // Destroy ringback (if need): conf, conf master port, null port, tonegen 

    

    

    if (g.exit_mutex)
    {
        status = pj_mutex_destroy (g.exit_mutex);
        if (status != PJ_SUCCESS)
            pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pj_mutex_destroy()");
    }

    if (g.nullport) 
        pjmedia_port_destroy (g.nullport); // CATCH


    if (g.tonegen_port)
        pjmedia_port_destroy (g.tonegen_port); // CATCH

    if (g.conf)
         pjmedia_conf_destroy (g.conf); // CATCH

    if (g.media_endpt)
    {
        status = pjmedia_endpt_destroy (g.media_endpt);
        if (status != PJ_SUCCESS)
            pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pjmedia_endpt_destroy()");
    }

    if (g.sip_endpt)
        pjsip_endpt_destroy (g.sip_endpt);

    if (g.pool)
        pj_pool_release (g.pool);
    
    pj_caching_pool_destroy (&g.cp);

    
    printf ("\n\n\n");
    PJ_LOG (5, (APPNAME, "Application has been finished normally"));
    return 0;
}