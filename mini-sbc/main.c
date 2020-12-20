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



static char *json_doc1;/* = 
"{\
    \"Object\": {\
       \"Integer\":  800,\
       \"Negative\":  -12,\
       \"Float\": -7.2,\
       \"String\":  \"A\\tString with tab\",\
       \"Object2\": {\
           \"True\": true,\
           \"False\": false,\
           \"Null\": null\
       },\
       \"Array1\": [116, false, \"string\", {}],\
       \"Array2\": [\
	    {\
        	   \"Float\": 123.,\
	    },\
	    {\
		   \"Float\": 123.,\
	    }\
       ]\
     },\
   \"Integer\":  800,\
   \"Array1\": [116, false, \"string\"]\
}\
";*/



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
    g.json_filename = "ms.json";
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

    if (argc > 1) // arguments required 
    {
    
    }
    else // parse JSON
    {
        const char *func = "JSON parsing";
        unsigned sip_in=0, sip_out=0, rtp_in=0, rtp_out=0;
        char ip_in[20] = {0}, ip_out[20] = {0};
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
                printf ("%d\n", t++);
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
                g.rtp2_start_port = rtp_out;
            if (rtp_in)
                g.rtp_start_port = rtp_in;
            if (sip_in)
                g.sip_port = sip_in;
            if (sip_out)
                g.sip_port2 = sip_out;
            if (ip_out[0] != '\0')
                printf ("Gotten IN ip %s\n", ip_out);
            return 0;
        }
        
    }

    
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