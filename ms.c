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
    enum state_t {BUSY=0, READY, DISABLED} state
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


    char            *local_uri;
    char            *local_contact;

    pj_uint16_t     sip_port;
    pj_uint16_t     rtp_port;

    junction_t      junctions[10];
    numrecord_t     numbook[20];

    pjmedia_conf    *tonegen_conf;
    pjmedia_port    *tonegen_port;
    unsigned        tonegen_port_id;

    pj_bool_t       pause;
    pj_bool_t       to_halt;
    pj_bool_t       to_exit;

    pjmedia_conf    *conf;
    pjmedia_master_port *conf_mp;
    pjmedia_port    *conf_null_port;
};


/////// FUNCTIONS

int console_thread (void *p);
void emergency_exit ();
void halt ();
void free_junction (junction_t *j);
void free_leg (leg_t *l);
void nullize_leg (leg_t *l);
void destroy_junction (junction_t *j);