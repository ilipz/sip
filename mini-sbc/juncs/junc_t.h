#ifndef JUNC_T_H
#define JUNC_T_H

#include "../types.h"

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
    leg_t          *reverse;    
    enum leg_type {IN=0, OUT=1} type;
} leg_t;

typedef struct junction
{
    leg_t  in_leg;
    leg_t  out_leg;
    pjmedia_master_port *mp_in_out, *mp_out_in;
    enum state_t {BUSY=0, READY, DISABLED} state;
    pj_mutex_t  *mutex;
    int index; 
} junction_t;

#endif
