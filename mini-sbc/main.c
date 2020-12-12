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
#include <getopt.h>


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
    {"05", "192.168.0.9:7060"},
    {"vedro", "192.168.0.3:15060"},
    {"ku", "192.168.40.220:15060"},
    {"vedro", "192.168.41.250:15060"}
};

int main (int argc, char **argv)
{
    PJ_LOG (5, (APPNAME, "Application has been started"));
    printf ("\n\n\n");
    static const char *THIS_FUNCTION = "main()";
    pj_status_t status;
    const pj_str_t empty = pj_str("empty"); 

    // TODO: Set global vars defaults
    g.sip_endpt = NULL;
    g.media_endpt = NULL;
    g.pool = NULL;
    g.tonegen_port = NULL;
    g.conf = NULL;
    g.conf_mp = NULL;
    g.conf_null_port = NULL;
    g.log_file = NULL;
    
    g.local_uri_in = empty;
    g.local_contact_in = empty;
    g.local_uri_out = empty;
    g.local_contact_out = empty;
    g.local_addr_in = empty;
    g.local_addr_out = empty;

    g.to_quit = 0;
    g.pause = 0;
    g.req_id = 0;

    g.sip_port_in = 5060;
    g.sip_port_out = 5060;
    g.rtp_port_out = 4000;
    g.rtp_port_in = 4000;

    // TODO: Parse args
    if (argc > 1)
    {
        //long rtp_port_, sip_port;
        pj_uint16_t tmp;
        while (1)
        {
            int option_symbol;
            int option_index;
            struct option long_options[] = 
            {
                {"sip-port-in", 1, 0, 1000},
                {"sip-port-out", 1, 0, 2000},
                {"rtp-port-in", 1, 0, 3000},
                {"rtp-port-out", 1, 0, 4000},
                {"ip-in", 1, 0, 5000},
                {"ip-out", 1, 0, 6000},
                {0, 0, 0, 0}
            };
            option_symbol = getopt_long_only (argc, argv, "~", long_options, &option_index);
            if (option_symbol == -1)
                break;
            if (!optarg)
            {
                printf ("\nMissed argument\n\n");
                return 0;
            }
                
            tmp = (pj_uint16_t) atol (optarg);
            switch (option_symbol)
            {
                case 1000:
                    if (tmp < 5060)
                    {
                        printf ("SIP port value cann't be less than 5060\n");
                        return 0;
                    }
                    g.sip_port_in = tmp;
                    break;
                
                case 2000:
                    if (tmp < 5060)
                    {
                        printf ("SIP port value cann't be less than 5060\n");
                        return 0;
                    }
                    g.sip_port_out = tmp;
                    break;

                case 3000:
                    if (tmp < 4000)
                    {
                        printf ("RTP port value cann't be less than 4000\n");
                        return 0;
                    }
                    g.rtp_port_in = tmp;
                    break;

                case 4000:
                    if (tmp < 4000)
                    {
                        printf ("RTP port value cann't be less than 4000\n");
                        return 0;
                    }
                    g.rtp_port_out = tmp;
                    break;
                
                case 5000:
                    g.local_addr_in = pj_str (optarg);
                    break;
                
                case 6000:
                    g.local_addr_out = pj_str (optarg);
                    break;
                
                default:
                    printf ("Invalid arguments\n");
                    return 0;


                    
            }
            if (option_symbol == -1)
                break;
        }
    }
    else
    {
        // TODO: parse json
    }
    
        
    
    

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

    init_exits ();

    

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

    if (g.conf_null_port) 
        pjmedia_port_destroy (g.conf_null_port); // CATCH


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