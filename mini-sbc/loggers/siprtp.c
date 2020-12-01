#include "siprtp.h"
extern struct global_var g;
const char *good_number(char *buf, pj_int32_t val)
{
    if (val < 1000) {
	pj_ansi_sprintf(buf, "%d", val);
    } else if (val < 1000000) {
	pj_ansi_sprintf(buf, "%d.%02dK", 
			val / 1000,
			(val % 1000) / 100);
    } else {
	pj_ansi_sprintf(buf, "%d.%02dM", 
			val / 1000000,
			(val % 1000000) / 10000);
    }

    return buf;
}



static void print_avg_stat(void)
{
#define MIN_(var,val)	   if ((int)val < (int)var) var = val
#define MAX_(var,val)	   if ((int)val > (int)var) var = val
#define AVG_(var,val)	   var = ( ((var * count) + val) / (count+1) )
#define BIGVAL		    0x7FFFFFFFL
    struct stat_entry
    {
	int min, avg, max;
    };

    struct stat_entry call_dur, call_pdd;
    pjmedia_rtcp_stat min_stat, avg_stat, max_stat;

    char srx_min[16], srx_avg[16], srx_max[16];
    char brx_min[16], brx_avg[16], brx_max[16];
    char stx_min[16], stx_avg[16], stx_max[16];
    char btx_min[16], btx_avg[16], btx_max[16];


    unsigned i, count;

    pj_bzero(&call_dur, sizeof(call_dur)); 
    call_dur.min = BIGVAL;

    pj_bzero(&call_pdd, sizeof(call_pdd)); 
    call_pdd.min = BIGVAL;

    pj_bzero(&min_stat, sizeof(min_stat));
    min_stat.rx.pkt = min_stat.tx.pkt = BIGVAL;
    min_stat.rx.bytes = min_stat.tx.bytes = BIGVAL;
    min_stat.rx.loss = min_stat.tx.loss = BIGVAL;
    min_stat.rx.dup = min_stat.tx.dup = BIGVAL;
    min_stat.rx.reorder = min_stat.tx.reorder = BIGVAL;
    min_stat.rx.jitter.min = min_stat.tx.jitter.min = BIGVAL;
    min_stat.rtt.min = BIGVAL;

    pj_bzero(&avg_stat, sizeof(avg_stat));
    pj_bzero(&max_stat, sizeof(max_stat));


    for (i=0, count=0; i<app.max_calls; ++i) {

	struct call *call = &app.call[i];
	struct media_stream *audio = &call->media[0];
	pj_time_val dur;
	unsigned msec_dur;

	if (call->inv == NULL || 
	    call->inv->state < PJSIP_INV_STATE_CONFIRMED ||
	    call->connect_time.sec == 0) 
	{
	    continue;
	}

	/* Duration */
	call_get_duration(i, &dur);
	msec_dur = PJ_TIME_VAL_MSEC(dur);

	MIN_(call_dur.min, msec_dur);
	MAX_(call_dur.max, msec_dur);
	AVG_(call_dur.avg, msec_dur);

	/* Connect delay */
	if (call->connect_time.sec) {
	    pj_time_val t = call->connect_time;
	    PJ_TIME_VAL_SUB(t, call->start_time);
	    msec_dur = PJ_TIME_VAL_MSEC(t);
	} else {
	    msec_dur = 10;
	}

	MIN_(call_pdd.min, msec_dur);
	MAX_(call_pdd.max, msec_dur);
	AVG_(call_pdd.avg, msec_dur);

	/* RX Statistisc: */

	/* Packets */
	MIN_(min_stat.rx.pkt, audio->rtcp.stat.rx.pkt);
	MAX_(max_stat.rx.pkt, audio->rtcp.stat.rx.pkt);
	AVG_(avg_stat.rx.pkt, audio->rtcp.stat.rx.pkt);

	/* Bytes */
	MIN_(min_stat.rx.bytes, audio->rtcp.stat.rx.bytes);
	MAX_(max_stat.rx.bytes, audio->rtcp.stat.rx.bytes);
	AVG_(avg_stat.rx.bytes, audio->rtcp.stat.rx.bytes);


	/* Packet loss */
	MIN_(min_stat.rx.loss, audio->rtcp.stat.rx.loss);
	MAX_(max_stat.rx.loss, audio->rtcp.stat.rx.loss);
	AVG_(avg_stat.rx.loss, audio->rtcp.stat.rx.loss);

	/* Packet dup */
	MIN_(min_stat.rx.dup, audio->rtcp.stat.rx.dup);
	MAX_(max_stat.rx.dup, audio->rtcp.stat.rx.dup);
	AVG_(avg_stat.rx.dup, audio->rtcp.stat.rx.dup);

	/* Packet reorder */
	MIN_(min_stat.rx.reorder, audio->rtcp.stat.rx.reorder);
	MAX_(max_stat.rx.reorder, audio->rtcp.stat.rx.reorder);
	AVG_(avg_stat.rx.reorder, audio->rtcp.stat.rx.reorder);

	/* Jitter  */
	MIN_(min_stat.rx.jitter.min, audio->rtcp.stat.rx.jitter.min);
	MAX_(max_stat.rx.jitter.max, audio->rtcp.stat.rx.jitter.max);
	AVG_(avg_stat.rx.jitter.mean, audio->rtcp.stat.rx.jitter.mean);


	/* TX Statistisc: */

	/* Packets */
	MIN_(min_stat.tx.pkt, audio->rtcp.stat.tx.pkt);
	MAX_(max_stat.tx.pkt, audio->rtcp.stat.tx.pkt);
	AVG_(avg_stat.tx.pkt, audio->rtcp.stat.tx.pkt);

	/* Bytes */
	MIN_(min_stat.tx.bytes, audio->rtcp.stat.tx.bytes);
	MAX_(max_stat.tx.bytes, audio->rtcp.stat.tx.bytes);
	AVG_(avg_stat.tx.bytes, audio->rtcp.stat.tx.bytes);

	/* Packet loss */
	MIN_(min_stat.tx.loss, audio->rtcp.stat.tx.loss);
	MAX_(max_stat.tx.loss, audio->rtcp.stat.tx.loss);
	AVG_(avg_stat.tx.loss, audio->rtcp.stat.tx.loss);

	/* Packet dup */
	MIN_(min_stat.tx.dup, audio->rtcp.stat.tx.dup);
	MAX_(max_stat.tx.dup, audio->rtcp.stat.tx.dup);
	AVG_(avg_stat.tx.dup, audio->rtcp.stat.tx.dup);

	/* Packet reorder */
	MIN_(min_stat.tx.reorder, audio->rtcp.stat.tx.reorder);
	MAX_(max_stat.tx.reorder, audio->rtcp.stat.tx.reorder);
	AVG_(avg_stat.tx.reorder, audio->rtcp.stat.tx.reorder);

	/* Jitter  */
	MIN_(min_stat.tx.jitter.min, audio->rtcp.stat.tx.jitter.min);
	MAX_(max_stat.tx.jitter.max, audio->rtcp.stat.tx.jitter.max);
	AVG_(avg_stat.tx.jitter.mean, audio->rtcp.stat.tx.jitter.mean);


	/* RTT */
	MIN_(min_stat.rtt.min, audio->rtcp.stat.rtt.min);
	MAX_(max_stat.rtt.max, audio->rtcp.stat.rtt.max);
	AVG_(avg_stat.rtt.mean, audio->rtcp.stat.rtt.mean);

	++count;
    }

    if (count == 0) {
	puts("No active calls");
	return;
    }

    printf("Total %d call(s) active.\n"
	   "                    Average Statistics\n"
	   "                    min     avg     max \n"
	   "                -----------------------\n"
	   " call duration: %7d %7d %7d %s\n"
	   " connect delay: %7d %7d %7d %s\n"
	   " RX stat:\n"
	   "       packets: %7s %7s %7s %s\n"
	   "       payload: %7s %7s %7s %s\n"
	   "          loss: %7d %7d %7d %s\n"
	   "  percent loss: %7.3f %7.3f %7.3f %s\n"
	   "           dup: %7d %7d %7d %s\n"
	   "       reorder: %7d %7d %7d %s\n"
	   "        jitter: %7.3f %7.3f %7.3f %s\n"
	   " TX stat:\n"
	   "       packets: %7s %7s %7s %s\n"
	   "       payload: %7s %7s %7s %s\n"
	   "          loss: %7d %7d %7d %s\n"
	   "  percent loss: %7.3f %7.3f %7.3f %s\n"
	   "           dup: %7d %7d %7d %s\n"
	   "       reorder: %7d %7d %7d %s\n"
	   "        jitter: %7.3f %7.3f %7.3f %s\n"
	   " RTT          : %7.3f %7.3f %7.3f %s\n"
	   ,
	   count,
	   call_dur.min/1000, call_dur.avg/1000, call_dur.max/1000, 
	   "seconds",

	   call_pdd.min, call_pdd.avg, call_pdd.max, 
	   "ms",

	   /* rx */

	   good_number(srx_min, min_stat.rx.pkt),
	   good_number(srx_avg, avg_stat.rx.pkt),
	   good_number(srx_max, max_stat.rx.pkt),
	   "packets",

	   good_number(brx_min, min_stat.rx.bytes),
	   good_number(brx_avg, avg_stat.rx.bytes),
	   good_number(brx_max, max_stat.rx.bytes),
	   "bytes",

	   min_stat.rx.loss, avg_stat.rx.loss, max_stat.rx.loss,
	   "packets",
	   
	   min_stat.rx.loss*100.0/(min_stat.rx.pkt+min_stat.rx.loss),
	   avg_stat.rx.loss*100.0/(avg_stat.rx.pkt+avg_stat.rx.loss),
	   max_stat.rx.loss*100.0/(max_stat.rx.pkt+max_stat.rx.loss),
	   "%",


	   min_stat.rx.dup, avg_stat.rx.dup, max_stat.rx.dup,
	   "packets",

	   min_stat.rx.reorder, avg_stat.rx.reorder, max_stat.rx.reorder,
	   "packets",

	   min_stat.rx.jitter.min/1000.0, 
	   avg_stat.rx.jitter.mean/1000.0, 
	   max_stat.rx.jitter.max/1000.0,
	   "ms",
	
	   /* tx */

	   good_number(stx_min, min_stat.tx.pkt),
	   good_number(stx_avg, avg_stat.tx.pkt),
	   good_number(stx_max, max_stat.tx.pkt),
	   "packets",

	   good_number(btx_min, min_stat.tx.bytes),
	   good_number(btx_avg, avg_stat.tx.bytes),
	   good_number(btx_max, max_stat.tx.bytes),
	   "bytes",

	   min_stat.tx.loss, avg_stat.tx.loss, max_stat.tx.loss,
	   "packets",
	   
	   min_stat.tx.loss*100.0/(min_stat.tx.pkt+min_stat.tx.loss),
	   avg_stat.tx.loss*100.0/(avg_stat.tx.pkt+avg_stat.tx.loss),
	   max_stat.tx.loss*100.0/(max_stat.tx.pkt+max_stat.tx.loss),
	   "%",

	   min_stat.tx.dup, avg_stat.tx.dup, max_stat.tx.dup,
	   "packets",

	   min_stat.tx.reorder, avg_stat.tx.reorder, max_stat.tx.reorder,
	   "packets",

	   min_stat.tx.jitter.min/1000.0, 
	   avg_stat.tx.jitter.mean/1000.0, 
	   max_stat.tx.jitter.max/1000.0,
	   "ms",

	   /* rtt */
	   min_stat.rtt.min/1000.0, 
	   avg_stat.rtt.mean/1000.0, 
	   max_stat.rtt.max/1000.0,
	   "ms"
	   );

}


static void print_call(int call_index)
{
    struct call *call = &app.call[call_index];
    int len;
    pjsip_inv_session *inv = call->inv;
    pjsip_dialog *dlg = inv->dlg;
    struct media_stream *audio = &call->media[0];
    char userinfo[PJSIP_MAX_URL_SIZE];
    char duration[80], last_update[80];
    char bps[16], ipbps[16], packets[16], bytes[16], ipbytes[16];
    unsigned decor;
    pj_time_val now;


    decor = pj_log_get_decor();
    pj_log_set_decor(PJ_LOG_HAS_NEWLINE);

    pj_gettimeofday(&now);

    if (app.report_filename)
	puts(app.report_filename);

    /* Print duration */
    if (inv->state >= PJSIP_INV_STATE_CONFIRMED && call->connect_time.sec) {

	PJ_TIME_VAL_SUB(now, call->connect_time);

	sprintf(duration, " [duration: %02ld:%02ld:%02ld.%03ld]",
		now.sec / 3600,
		(now.sec % 3600) / 60,
		(now.sec % 60),
		now.msec);

    } else {
	duration[0] = '\0';
    }



    /* Call number and state */
    PJ_LOG(3, (THIS_FILE,
	      "Call #%d: %s%s", 
	      call_index, pjsip_inv_state_name(inv->state), 
	      duration));



    /* Call identification */
    len = pjsip_hdr_print_on(dlg->remote.info, userinfo, sizeof(userinfo));
    if (len < 0)
	pj_ansi_strcpy(userinfo, "<--uri too long-->");
    else
	userinfo[len] = '\0';

    PJ_LOG(3, (THIS_FILE, "   %s", userinfo));


    if (call->inv == NULL || call->inv->state < PJSIP_INV_STATE_CONFIRMED ||
	call->connect_time.sec == 0) 
    {
	pj_log_set_decor(decor);
	return;
    }


    /* Signaling quality */
    {
	char pdd[64], connectdelay[64];
	pj_time_val t;

	if (call->response_time.sec) {
	    t = call->response_time;
	    PJ_TIME_VAL_SUB(t, call->start_time);
	    sprintf(pdd, "got 1st response in %ld ms", PJ_TIME_VAL_MSEC(t));
	} else {
	    pdd[0] = '\0';
	}

	if (call->connect_time.sec) {
	    t = call->connect_time;
	    PJ_TIME_VAL_SUB(t, call->start_time);
	    sprintf(connectdelay, ", connected after: %ld ms", 
		    PJ_TIME_VAL_MSEC(t));
	} else {
	    connectdelay[0] = '\0';
	}

	PJ_LOG(3, (THIS_FILE, 
		   "   Signaling quality: %s%s", pdd, connectdelay));
    }


    PJ_LOG(3, (THIS_FILE,
	       "   Stream #0: audio %.*s@%dHz, %dms/frame, %sB/s (%sB/s +IP hdr)",
   	(int)audio->si.fmt.encoding_name.slen,
	audio->si.fmt.encoding_name.ptr,
	audio->clock_rate,
	audio->samples_per_frame * 1000 / audio->clock_rate,
	good_number(bps, audio->bytes_per_frame * audio->clock_rate / audio->samples_per_frame),
	good_number(ipbps, (audio->bytes_per_frame+32) * audio->clock_rate / audio->samples_per_frame)));

    if (audio->rtcp.stat.rx.update_cnt == 0)
	strcpy(last_update, "never");
    else {
	pj_gettimeofday(&now);
	PJ_TIME_VAL_SUB(now, audio->rtcp.stat.rx.update);
	sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
		now.sec / 3600,
		(now.sec % 3600) / 60,
		now.sec % 60,
		now.msec);
    }

    PJ_LOG(3, (THIS_FILE, 
	   "              RX stat last update: %s\n"
	   "                 total %s packets %sB received (%sB +IP hdr)%s\n"
	   "                 pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)%s\n"
	   "                       (msec)    min     avg     max     last\n"
	   "                 loss period: %7.3f %7.3f %7.3f %7.3f%s\n"
	   "                 jitter     : %7.3f %7.3f %7.3f %7.3f%s",
	   last_update,
	   good_number(packets, audio->rtcp.stat.rx.pkt),
	   good_number(bytes, audio->rtcp.stat.rx.bytes),
	   good_number(ipbytes, audio->rtcp.stat.rx.bytes + audio->rtcp.stat.rx.pkt * 32),
	   "",
	   audio->rtcp.stat.rx.loss,
	   audio->rtcp.stat.rx.loss * 100.0 / (audio->rtcp.stat.rx.pkt + audio->rtcp.stat.rx.loss),
	   audio->rtcp.stat.rx.dup, 
	   audio->rtcp.stat.rx.dup * 100.0 / (audio->rtcp.stat.rx.pkt + audio->rtcp.stat.rx.loss),
	   audio->rtcp.stat.rx.reorder, 
	   audio->rtcp.stat.rx.reorder * 100.0 / (audio->rtcp.stat.rx.pkt + audio->rtcp.stat.rx.loss),
	   "",
	   audio->rtcp.stat.rx.loss_period.min / 1000.0, 
	   audio->rtcp.stat.rx.loss_period.mean / 1000.0, 
	   audio->rtcp.stat.rx.loss_period.max / 1000.0,
	   audio->rtcp.stat.rx.loss_period.last / 1000.0,
	   "",
	   audio->rtcp.stat.rx.jitter.min / 1000.0,
	   audio->rtcp.stat.rx.jitter.mean / 1000.0,
	   audio->rtcp.stat.rx.jitter.max / 1000.0,
	   audio->rtcp.stat.rx.jitter.last / 1000.0,
	   ""
	   ));


    if (audio->rtcp.stat.tx.update_cnt == 0)
	strcpy(last_update, "never");
    else {
	pj_gettimeofday(&now);
	PJ_TIME_VAL_SUB(now, audio->rtcp.stat.tx.update);
	sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
		now.sec / 3600,
		(now.sec % 3600) / 60,
		now.sec % 60,
		now.msec);
    }

    PJ_LOG(3, (THIS_FILE,
	   "              TX stat last update: %s\n"
	   "                 total %s packets %sB sent (%sB +IP hdr)%s\n"
	   "                 pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)%s\n"
	   "                       (msec)    min     avg     max     last\n"
	   "                 loss period: %7.3f %7.3f %7.3f %7.3f%s\n"
	   "                 jitter     : %7.3f %7.3f %7.3f %7.3f%s",
	   last_update,
	   good_number(packets, audio->rtcp.stat.tx.pkt),
	   good_number(bytes, audio->rtcp.stat.tx.bytes),
	   good_number(ipbytes, audio->rtcp.stat.tx.bytes + audio->rtcp.stat.tx.pkt * 32),
	   "",
	   audio->rtcp.stat.tx.loss,
	   audio->rtcp.stat.tx.loss * 100.0 / (audio->rtcp.stat.tx.pkt + audio->rtcp.stat.tx.loss),
	   audio->rtcp.stat.tx.dup, 
	   audio->rtcp.stat.tx.dup * 100.0 / (audio->rtcp.stat.tx.pkt + audio->rtcp.stat.tx.loss),
	   audio->rtcp.stat.tx.reorder, 
	   audio->rtcp.stat.tx.reorder * 100.0 / (audio->rtcp.stat.tx.pkt + audio->rtcp.stat.tx.loss),
	   "",
	   audio->rtcp.stat.tx.loss_period.min / 1000.0, 
	   audio->rtcp.stat.tx.loss_period.mean / 1000.0, 
	   audio->rtcp.stat.tx.loss_period.max / 1000.0,
	   audio->rtcp.stat.tx.loss_period.last / 1000.0,
	   "",
	   audio->rtcp.stat.tx.jitter.min / 1000.0,
	   audio->rtcp.stat.tx.jitter.mean / 1000.0,
	   audio->rtcp.stat.tx.jitter.max / 1000.0,
	   audio->rtcp.stat.tx.jitter.last / 1000.0,
	   ""
	   ));


    PJ_LOG(3, (THIS_FILE,
	   "             RTT delay      : %7.3f %7.3f %7.3f %7.3f%s\n", 
	   audio->rtcp.stat.rtt.min / 1000.0,
	   audio->rtcp.stat.rtt.mean / 1000.0,
	   audio->rtcp.stat.rtt.max / 1000.0,
	   audio->rtcp.stat.rtt.last / 1000.0,
	   ""
	   ));

    pj_log_set_decor(decor);
}
