#include "inits.h"
extern struct global_var g;
pjsip_module tmp1 = 
{
    NULL, NULL,			    /* prev, next.		*/
    { "App module", 10 },	    /* Name.			*/
    -1,				    /* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION, /* Priority			*/
    NULL,			    /* load()			*/
    NULL,			    /* start()			*/
    NULL,			    /* stop()			*/
    NULL,			    /* unload()			*/
    &on_rx_request,		    /* on_rx_request()		*/
    NULL,			    /* on_rx_response()		*/  
    NULL,			    /* on_tx_request.		*/ //here may be ACK is sent
    NULL,			    /* on_tx_response()		*/
    NULL,			    /* on_tsx_state()		*/
};

 pjsip_module tmp2 = 
{
    NULL, NULL,				/* prev, next.		*/
    { "Logger module", 14 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &logger_rx_msg,			/* on_rx_request()	*/
    &logger_rx_msg,			/* on_rx_response()	*/
    &logger_tx_msg,			/* on_tx_request.	*/
    &logger_tx_msg,			/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};

void init_sip()
{
    unsigned i;
    pj_status_t status;

    g.mod_app = tmp1;
    g.mod_logger = tmp2;

    /* Create the endpoint: */
    status = pjsip_endpt_create(&g.cp.factory, pj_gethostname()->ptr, 
				&g.sip_endpt);
    if (status != PJ_SUCCESS)
        exit (80);

    g.sip_port = 5060;
    g.rtp_port = 4000;
    /* Add UDP transport. */
    {
	pj_sockaddr_in addr;
	pjsip_host_port addrname;
	pjsip_transport *tp;

	pj_bzero(&addr, sizeof(addr));
	addr.sin_family = pj_AF_INET();
	addr.sin_addr.s_addr = 0;
	addr.sin_port = pj_htons((pj_uint16_t)g.sip_port); 

	if (g.local_addr.slen) {

	    addrname.host = g.local_addr;
	    addrname.port = g.sip_port;

	    status = pj_sockaddr_in_init(&addr, &g.local_addr, 
					 (pj_uint16_t)g.sip_port);
        if (status != PJ_SUCCESS)
        exit (80);
	    
	}

	status = pjsip_udp_transport_start( g.sip_endpt, &addr, 
					    (g.local_addr.slen ? &addrname:NULL),
					    2, &tp);
	
    if (status != PJ_SUCCESS)
        exit (80);

	PJ_LOG(3,(APPNAME, "SIP UDP listening on %.*s:%d",
		  (int)tp->local_name.host.slen, tp->local_name.host.ptr,
		  tp->local_name.port));
    }

    /* 
     * Init transaction layer.
     * This will create/initialize transaction hash tables etc.
     */
    status = pjsip_tsx_layer_init_module(g.sip_endpt);
    if (status != PJ_SUCCESS)
        exit (80);

    /*  Initialize UA layer. */
    status = pjsip_ua_init_module( g.sip_endpt, NULL );
    if (status != PJ_SUCCESS)
        exit (80);

    /* Initialize 100rel support */
    status = pjsip_100rel_init_module(g.sip_endpt);
    if (status != PJ_SUCCESS)
        exit (80);

    /*  Init invite session module. */
    {
	pjsip_inv_callback inv_cb;

	/* Init the callback for INVITE session: */
	pj_bzero(&inv_cb, sizeof(inv_cb));
	inv_cb.on_state_changed = &on_state_changed;
	inv_cb.on_new_session = &on_forked;
	inv_cb.on_media_update = &on_media_update;
	//inv_cb.on_send_ack = &on_send_ack;

	/* Initialize invite session module:  */
	status = pjsip_inv_usage_init(g.sip_endpt, &inv_cb);
	if (status != PJ_SUCCESS)
    {
        pj_perror (5, "nu ty pone", status, "wot tak");
        exit (1488);
    }
    }

    /* Register our module to receive incoming requests. */
    status = pjsip_endpt_register_module( g.sip_endpt, &g.mod_app);
    if (status != PJ_SUCCESS)
        exit (80);
    //PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_endpt_register_module(g.sip_endpt, &g.mod_logger);
    if (status != PJ_SUCCESS)
        exit (80);
    /* Done */
    
}

pj_bool_t init_media()
{
    pj_status_t	status;

    /* Initialize media endpoint so that at least error subsystem is properly
     * initialized.
     */

    status = pjmedia_endpt_create(&g.cp.factory, NULL, 1, &g.media_endpt);

    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Must register codecs to be supported */

    pjmedia_codec_g711_init(g.media_endpt);

}

void init_juncs ()
{
    int dis_count=0;
    const char *THIS_FUNCTION = "init_juncs()";
    /////
    pj_status_t status;
    pj_uint16_t rtp_port = (pj_uint16_t)(g.rtp_start_port & 0xFFFE);

    for (int current_junc=0; current_junc<10; current_junc++)
    {
        junction_t *j = &g.junctions[current_junc];
        j->index = current_junc;
        j->state = DISABLED;

        nullize_leg (&j->in_leg);
        j->in_leg.type = IN;
        j->in_leg.reverse = &j->out_leg;
        j->in_leg.junction = j;
        

        nullize_leg (&j->out_leg);
        j->out_leg.type = OUT;
        j->out_leg.reverse = &j->in_leg;
        j->out_leg.junction = j;
        
        for (int retry=0; retry<=1500; retry++)
        {
            status = pjmedia_transport_udp_create2 (g.media_endpt, "in_leg", &g.local_addr, rtp_port++, 0, &j->in_leg.media_transport);
            if (PJ_SUCCESS == status)
            {
                //pj_perror (5, "init media()", status, "pjmedia_transport_udp_create2");
                j->state = READY;
                break;
            }
        }

        if (j->state == DISABLED)
        {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_transport_udp_create2()");
            continue;
        }
            
        for (int retry=0; retry<=1500; retry++)
        {
            status = pjmedia_transport_udp_create2 (g.media_endpt, "in_leg", &g.local_addr, rtp_port++, 0, &j->out_leg.media_transport);
            if (PJ_SUCCESS == status)
            {
                //pj_perror (5, THIS_FUNCTION, status, "pjmedia_transport_udp_create2");
                j->state = READY;
                break;
            }

        }

        if (j->state == DISABLED)
        {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_transport_udp_create2()");
            pjmedia_transport_close (j->in_leg.media_transport);
            continue;
        }
        
        
    }
}
