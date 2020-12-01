#ifndef TYPES_H
#define TYPES_H

#include <pjsip.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <pthread.h>
#include <stdlib.h>
#include "juncs/junc_t.h"

#define PJ_LOG_ERROR    "!!! ERROR: " 
#define APPNAME         "MINI-SBC: "

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
            pj_bool_t sdp_neg_done;
        } current;
    
    pjmedia_transport   *media_transport;
    struct leg         *reverse;
    void  *junction;
    enum leg_type {IN=0, OUT=1} type;
} leg_t;

typedef struct junction
{
    leg_t  in_leg;
    leg_t  out_leg;
    pjmedia_master_port *mp_in_out, *mp_out_in;
    pj_thread_t *controller_thread;
    enum state_t {BUSY=0, READY, DISABLED} state;
    pj_mutex_t  *mutex;
    int index; 
} junction_t;

struct codec
{
    unsigned	pt;
    char*	name;
    unsigned	clock_rate;
    unsigned	bit_rate;
    unsigned	ptime;
    char*	description;
};

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

    pjsip_module mod_logger;
    pjsip_module mod_app;
    pj_str_t		 local_uri;
    pj_str_t		 local_contact;
    pj_str_t		 local_addr;

    pj_uint16_t     sip_port;
    pj_uint16_t     rtp_port;

    junction_t      junctions[10];
    numrecord_t     numbook[20];

    pjmedia_conf    *tonegen_conf;
    pjmedia_port    *tonegen_port;
    unsigned        tonegen_port_id;

    pj_bool_t       pause;

    pjmedia_conf    *conf;
    pjmedia_master_port *conf_mp;
    pjmedia_port    *conf_null_port;
    FILE *log_file;
    unsigned rtp_start_port;

    struct codec audio_codec;
    int to_quit;

};



#endif
