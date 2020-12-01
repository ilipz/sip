#include "types.h"
#include "cb/forked.h"
#include "cb/media_upd.h"
#include "cb/rx_req.h"
#include "cb/state_chd.h"
#include "juncs/free.h"
#include "juncs/junc_t.h"
#include "loggers/rx_msg.h"
#include "loggers/siprtp.h"
#include "loggers/tx_msg.h"
#include "threads/console.h"
#include "threads/junc_controller.h"
#include "utils/exits.h"
#include "utils/inits.h"
#include "utils/util.h"

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

int main (int argc, char **argv)
{
    pj_status_t status;

    pj_init ();
    // TODO: catch errors

    pjlib_util_init();
    // TODO: catch errors

    pj_caching_pool_init(&g.cp, &pj_pool_factory_default_policy, 0);

    
    g.pool = pj_pool_create(&g.cp.factory, "app", 4000, 4000, NULL);    

    g.to_quit = 0;
    g.pause = 0;
    // TODO: parse options and json
    init_sip ();
    
    pjsip_endpt_register_module(g.sip_endpt, &g.mod_logger);
    // TODO: catch errors

    

    init_media ();

    init_juncs ();
    // TODO: catch errors

    pj_time_val timeout = {0, 10};
    while (!g.to_quit)
    {
        pjsip_endpt_handle_events (g.sip_endpt, &timeout);
        while (g.pause) sleep(1); 
    }

    // Release resources

    return 0;
}