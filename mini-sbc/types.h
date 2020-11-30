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

#define PJ_LOG_ERROR    "!!! ERROR: " 
#define APPNAME         "MINI-SBC: "

struct codec
{
    unsigned	pt;
    char*	name;
    unsigned	clock_rate;
    unsigned	bit_rate;
    unsigned	ptime;
    char*	description;
};

struct global_var
{
    pjsip_endpoint  *sip_endpt;
    pjmedia_endpt   *media_endpt;

    pj_caching_pool cp;
    pj_pool_t       *pool;


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

typedef struct numrecord
{
	char num[8];
	char addr[32];
} numrecord_t;

#endif
