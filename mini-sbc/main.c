/*
    ROADMAP:
    1) Fixes: segfaults, catching bad PJ returned status, log 
    2) JSON: add telnums array reading
    3) Different net interfaces
    -------- Optional
    + Ringback (KPV)
    + Direct RTP stream (faster and no codecs requires)
    + On/off leg media streams; record to .wav
    + Extended logging: log to file, logging from siprtp.c
    + Threads: free junctions in other thread
-----------------------------------------------------------------
- So problems with pjmedia_frames in transport. Should try close media transport after any call

-----------------------------------------------------------------
Down references how work in simple JSON notation file
{
    "begin": "begin", <---- start of list (head->value.children.next; _||_.next - to get 2nd elem of list and etc.) 
    ................,
    "end": "end"      <---- finish of list (head->value.children.prev; _||_.prev - to get LAST-1 elem of list and etc.); last->next==NULL
}

*/

/*
    Arguments (keys in JSON config too):
    ip-in
    ip-out
    sip-in
    sip-out
    rtp-in
    rtp-out
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





/*struct codec audio_codecs[] = 
{
    { 0,  "PCMU", 8000, 16000, 20, "G.711 ULaw" },
    { 3,  "GSM",  8000, 13200, 20, "GSM" },
    { 4,  "G723", 8000, 6400,  30, "G.723.1" },
    { 8,  "PCMA", 8000, 16000, 20, "G.711 ALaw" },
    { 18, "G729", 8000, 8000,  20, "G.729" },
}; */

struct global_var g;


numrecord_t nums[4] = 
{
    {"05", "127.0.0.1:5060"},
    {"777", "127.0.0.1:5060"},
    {"9000", "127.0.0.1:5060"},
    {"1234", "127.0.0.1:5060"}
};

int main (int argc, char **argv)
{
    PJ_LOG (5, (APPNAME, "Application has been started"));
    printf ("\n\n\n");
    
    static const char *THIS_FUNCTION = "main()";
    static const char *json_default = "ms.json";
    pj_status_t status;
    
    // set defaults
    g.mode = ONE_NETWORK;
    g.sip_endpt = NULL;
    g.media_endpt = NULL;
    g.pool = NULL;
    g.nullport = NULL;
    g.json_filename = json_default;
    g.sip_port = 5060;
    g.sip_port2 = 5060;
    g.numlist = &nums;
    g.numlist_q = 4;
    g.tonegen_conf = NULL;
    g.tonegen_port = NULL;
    g.tonegen_port_id = 0;
    g.pause = 0;
    g.log_file = NULL;
    g.rtp_start_port2 = 4000;
    g.rtp_start_port = 4000;
    g.to_quit = 0;
    g.exit_mutex = NULL;

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

    

    g.pool = pj_pool_create(&g.cp.factory, "mini-sbc mempool", 4000, 4000, NULL);    
    if (g.pool == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"global pool hasn't been created (pool pointer == NULL). App halted"));
        halt ("pj_pool_create()");
    }



    if (argc > 1) // if there are any arguments 
    {
        if (strcmp (argv[1], "-f") == 0) // if first argument sets json file 
        {
            g.json_filename = argv[2];
        }
        else if (strcmp (argv[1], "-d") == 0)
        {
            printf ("Using defaults\n");
        }
        else //parse arguments, don't search json file
        {
            g.json_filename = NULL;
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
                        g.sip_port = tmp;
                        break;
                
                    case 2000:
                        if (tmp < 5060)
                        {
                            printf ("SIP port value cann't be less than 5060\n");
                            return 0;
                        }
                        g.sip_port2 = tmp;
                        break;

                    case 3000:
                        if (tmp < 4000)
                        {
                            printf ("RTP port value cann't be less than 4000\n");
                            return 0;
                        }
                        g.rtp_start_port = tmp;
                        break;

                    case 4000:
                        if (tmp < 4000)
                        {
                            printf ("RTP port value cann't be less than 4000\n");
                            return 0;
                        }
                        g.rtp_start_port2 = tmp;
                        break;
                
                    case 5000:
                        g.local_addr = pj_str (optarg);
                        break;
                
                    case 6000:
                        g.local_addr2 = pj_str (optarg);
                        break;

                    default:
                        printf ("USAGE\n");
                        return 0;


                    
                }
                if (option_symbol == -1)
                    break;
            }
        }
    }
        
    

    if (g.json_filename != NULL) // if JSON file specifed
    {
        const char *func = "JSON parsing";
        static char *json_doc1;
        unsigned sip_in=0, sip_out=0, rtp_in=0, rtp_out=0;
        char ip_in[20] = {0}, ip_out[20] = {0};
        pj_str_t *_ip_in, *_ip_out;
        pj_json_elem *elem;
        //char *out_buf;
        pj_json_err_info err;
        FILE *json_file = fopen (g.json_filename, "rt");
        if (json_file == NULL)
            emergency_exit ("Cann't open json config", NULL);
        fseek (json_file, 0L, SEEK_END);
        long size = ftell (json_file);
        rewind (json_file);
        json_doc1 = pj_pool_zalloc (g.pool, size+10);
        if (json_doc1 == NULL)
            emergency_exit (func, NULL);
        fread (json_doc1, sizeof(char), size+1, json_file);
        //json_doc1[size+1] = '\0';
        
        //sleep (10);
        //size = (unsigned)strlen(json_doc1);
        //printf ("\n%s\n\n", json_doc1);
        elem = pj_json_parse(g.pool, json_doc1, &size, &err);
        //sleep (10);
        if (elem == NULL) 
        {
	        PJ_LOG(5, (func, PJ_LOG_ERROR"error parsing %s. Using defaults", g.json_filename));
	        
        }
        else
        {
            pj_json_elem *head = elem->value.children.next;
            int t=0;
            while (1)
            {
                
                if (! (head->type != PJ_JSON_VAL_STRING || head->type != PJ_JSON_VAL_NUMBER) )
                {
                    PJ_LOG (5, (func, PJ_LOG_ERROR"Invalid value. Skip '%.*s' key", head->name.slen, head->name.ptr));
                    head = head->next;continue;
                }

                if (pj_strcmp2(&head->name, "ip-in") == 0)
                {
                    if (pj_strlen (&head->value.str) > 15)
                    {
                        PJ_LOG (5, (func, PJ_LOG_ERROR"Invalid value. Must be like 255.255.255.255. Skip '%.*s' key", head->name.slen, head->name.ptr));
                        head = head->next;continue;
                    }
                    pj_memcpy (ip_in, head->value.str.ptr, head->value.str.slen);
                    _ip_in = &head->value.str;
                    head = head->next;continue;
                }

                else if (pj_strcmp2(&head->name, "ip-out") == 0)
                {
                    if (pj_strlen (&head->value.str) > 15)
                    {
                        PJ_LOG (5, (func, PJ_LOG_ERROR"Invalid value. Must be like 255.255.255.255. Skip '%.*s' key", head->name.slen, head->name.ptr));
                        head = head->next;continue;
                    }
                    pj_memcpy (ip_out, head->value.str.ptr, head->value.str.slen);
                    _ip_out = &head->value.str;
                    head = head->next;continue;
                }

                else if (pj_strcmp2(&head->name, "sip-in") == 0)
                {
                    if (head->value.num < 5060.0 || head->value.num > 65536.0)
                    {
                        PJ_LOG (5, (func, PJ_LOG_ERROR"Invalid value. Must be in [5060; 65536]. Skip '%.*s' key", head->name.slen, head->name.ptr));
                        head = head->next;continue;
                    }
                    sip_in = (unsigned) head->value.num;
                    head = head->next;continue;
                }

                else if (pj_strcmp2(&head->name, "sip-out") == 0)
                {
                    if (head->value.num < 5060.0 || head->value.num > 65536.0)
                    {
                        PJ_LOG (5, (func, PJ_LOG_ERROR"Invalid value. Must be in [5060; 65536]. Skip '%.*s' key", head->name.slen, head->name.ptr));
                        head = head->next;continue;
                    }
                    sip_out = (unsigned) head->value.num;
                    head = head->next;continue;
                }

                else if (pj_strcmp2(&head->name, "rtp-out") == 0)
                {
                    if (head->value.num < 4000.0 || head->value.num > 65536.0)
                    {
                        PJ_LOG (5, (func, PJ_LOG_ERROR"Invalid value. Must be in [4000; 65536]. Skip '%.*s' key", head->name.slen, head->name.ptr));
                        head = head->next;continue;
                    }
                    rtp_out = (unsigned) head->value.num;
                    head = head->next;continue;
                }

                else if (pj_strcmp2(&head->name, "rtp-in") == 0)
                {
                    if (head->value.num < 4000.0 || head->value.num > 65536.0)
                    {
                        PJ_LOG (5, (func, PJ_LOG_ERROR"Invalid value. Must be in [4000; 65536]. Skip '%.*s' key", head->name.slen, head->name.ptr));
                        head = head->next;continue;
                    }
                    rtp_in = (unsigned) head->value.num;
                    head = head->next;continue;
                }

                else if (pj_strcmp2(&head->name, "telnums") == 0)
                {
                    if (head->type != PJ_JSON_VAL_OBJ)
                    {
                        PJ_LOG (5, (func, PJ_LOG_ERROR"Invalid \"telnums\" type. Must be an obj. Skip"));
                        head = head->next;continue;
                    }
                    pj_json_elem *tel_head = head->value.children.next;
                    if (pj_strcmp2 (&tel_head->name, "q") == 0) // if first pair is "q"anity and value
                    {
                        if (tel_head->type == PJ_JSON_VAL_NUMBER)
                        {
                            if (tel_head->value.num < 1) // use <"q": 0> to disable numbook in JSON and use default one 
                            {
                                continue;
                            } 
                            g.numlist = pj_pool_zalloc (g.pool, sizeof(numrecord_t) * (int)tel_head->value.num);
                            g.numlist_q = (int)tel_head->value.num; 
                            for (int i=0; i<g.numlist_q; i++)
                            {
                                tel_head = tel_head->next;
                                if (pj_strcmp2 (&tel_head->name, "end") == 0)
                                {
                                    g.numlist_q = i;
                                    break;
                                }
                                pj_memcpy (g.numlist[i].num, tel_head->name.ptr, tel_head->name.slen);
                                pj_memcpy (g.numlist[i].addr, tel_head->value.str.ptr, tel_head->value.str.slen);
                                  
                            }
                        }
                    }
                    else
                    {
                        PJ_LOG (5, (func, PJ_LOG_ERROR"Invalid first pair in obj 'telnums' - must be:\n\"q\": <telephone numbers quanity>"));
                    }
                    
                    head = head->next;continue;
                }

                else if (pj_strcmp2(&head->name, "end") == 0)
                    break;
                
                else 
                {
                    PJ_LOG (5, (func, PJ_LOG_ERROR"Invalid key: '%.*s'. Skip", head->name.slen, head->name.ptr));
                    head = head->next;continue;
                }

                head = head->next;
                if (head == NULL)
                    break;
            }
            if (rtp_out)
            {
                g.rtp_start_port2 = rtp_out;
                printf ("RTP OUT port: %d\n", rtp_out);
            }
                
            if (rtp_out)
            {
                g.rtp_start_port = rtp_in;
                printf ("RTP IN port: %d\n", rtp_in);
            }
            if (sip_in)
                g.sip_port = sip_in;
            if (sip_out)
                g.sip_port2 = sip_out;
            if (ip_out[0] != '\0')
            {
                g.local_addr2 = pj_str (ip_out);
            }
            if (ip_in[0] != '\0')
            {
                g.local_addr = *_ip_in;
            }
                printf ("Gotten OUT ip %s\n", ip_out);

            for (int i=0; i<g.numlist_q; i++)
                printf ("%s --> %s\n", g.numlist[i].num, g.numlist[i].addr);
            //return 0;
        }
        
    } // finish parse json

    
/////////////////////////////////////////////////////////////////////
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