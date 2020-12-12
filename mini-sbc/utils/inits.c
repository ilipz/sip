#include "inits.h"

extern struct global_var g;

static pjsip_module tmp1 = 
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

 static pjsip_module tmp2 = 
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
    static const char *THIS_FUNCTION = "init_sip()";
    unsigned i;
    pj_status_t status;

    g.mod_app = tmp1;
    g.mod_logger = tmp2;

    /* Create the endpoint: */
    status = pjsip_endpt_create(&g.cp.factory, pj_gethostname()->ptr, &g.sip_endpt);
    if (status != PJ_SUCCESS)
        emergency_exit ("init_sip()::pjsip_endpt_create()", &status);
       

    
    /* Add UDP transport. */
    {
	pj_sockaddr_in addr, addr2;
	pjsip_host_port addrname, addrname2;
	pjsip_transport *tp;

	pj_bzero(&addr, sizeof(addr));
	addr.sin_family = pj_AF_INET();
	addr.sin_addr.s_addr = 0;
	addr.sin_port = pj_htons((pj_uint16_t)g.sip_port_in); 

    pj_bzero(&addr2, sizeof(addr2));
	addr2.sin_family = pj_AF_INET();
	addr2.sin_addr.s_addr = 0;
	addr2.sin_port = pj_htons((pj_uint16_t)g.sip_port_out); 

    

    addrname2.host = g.local_addr_out;
    addrname2.port = g.sip_port_out;

    

    char local_uri[64];
    pj_str_t *tmp_addr_in = &g.local_addr_in;

	if (!pj_strcmp2(&g.local_addr_in, "empty"))
        tmp_addr_in = NULL;
     
    

	    addrname.host = g.local_addr_in;
	    addrname.port = g.sip_port_in;

	    status = pj_sockaddr_in_init(&addr, tmp_addr_in, (pj_uint16_t)g.sip_port_in);
        if (status != PJ_SUCCESS)
            emergency_exit ("init_sip()::pj_sockaddr_in_init()", &status);
        
        status = pjsip_udp_transport_start( g.sip_endpt, &addr, (tmp_addr_in ? &addrname:NULL), 2, &tp);
        if (status != PJ_SUCCESS)
            emergency_exit ("init_sip()::pjsip_udp_transport_start()", &status);

	    PJ_LOG(3,(APPNAME, "SIP UDP listening on %.*s:%d",
		  (int)tp->local_name.host.slen, tp->local_name.host.ptr,
		  tp->local_name.port));

        g.local_addr_in = tp->local_name.host;

        pj_ansi_sprintf (local_uri, "sip:%s:%d", g.local_addr_in.ptr, g.sip_port_in);
        memcpy (g.local_uri_in_c, local_uri, sizeof(local_uri));
        g.local_contact_in = pj_str (g.local_uri_in_c);
        g.local_uri_in = g.local_contact_in;
        


    if (pj_strcmp2(&g.local_addr_out, "empty"))
    {
        status = pj_sockaddr_in_init(&addr2, &g.local_addr_out, (pj_uint16_t)g.sip_port_out);
        if (status != PJ_SUCCESS)
            emergency_exit ("init_sip()::pj_sockaddr_in_init() 2", &status);

        status = pjsip_udp_transport_start( g.sip_endpt, &addr2, &addrname2, 2, &tp);
        if (status != PJ_SUCCESS)
            emergency_exit ("init_sip()::pjsip_udp_transport_start() 2", &status);

        pj_ansi_sprintf (local_uri, "sip:%s:%d", g.local_addr_out.ptr, g.sip_port_out);
        memcpy (g.local_uri_out_c, local_uri, sizeof(local_uri));
        g.local_contact_out = pj_str (g.local_uri_out_c);
        g.local_uri_in = g.local_contact_out;
    }
    else
    {
        PJ_LOG (5, (THIS_FUNCTION, PJ_LOG_ERROR"OUT net interface isn't defined. Using IN net interface for both legs"));

        g.local_uri_out = g.local_uri_in;
        g.local_contact_out = g.local_contact_in;
        g.local_addr_out = g.local_addr_in;
        g.rtp_port_out = g.rtp_port_in;
    }


    }
    /* 
     * Init transaction layer.
     * This will create/initialize transaction hash tables etc.
     */
    status = pjsip_tsx_layer_init_module(g.sip_endpt);
    if (status != PJ_SUCCESS)
       emergency_exit ("init_sip()::pjsip_tsx_layer_init_module()", &status);

    /*  Initialize UA layer. */
    status = pjsip_ua_init_module( g.sip_endpt, NULL );
    if (status != PJ_SUCCESS)
        emergency_exit ("init_sip()::pjsip_ua_init_module()", &status);

    /* Initialize 100rel support */
    status = pjsip_100rel_init_module(g.sip_endpt);
    if (status != PJ_SUCCESS)
        emergency_exit ("init_sip()::pjsip_100rel_init_module()", &status);

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
        emergency_exit ("init_sip()::pjsip_inv_usage_init()", &status);
    }

    /* Register our module to receive incoming requests. */
    status = pjsip_endpt_register_module( g.sip_endpt, &g.mod_app);
    if (status != PJ_SUCCESS)
        emergency_exit ("init_sip()::pjsip_endpt_register_module()", &status);
    //PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    status = pjsip_endpt_register_module(g.sip_endpt, &g.mod_logger);
    if (status != PJ_SUCCESS)
        emergency_exit ("init_sip()::pjsip_endpt_register_module()", &status);
    
    
    
    /* Done */

    
}

pj_bool_t init_media()
{
    pj_status_t	status;

    /* Initialize media endpoint so that at least error subsystem is properly
     * initialized.
     */

    status = pjmedia_endpt_create(&g.cp.factory, NULL, 1, &g.media_endpt);

    if (status != PJ_SUCCESS)
        emergency_exit ("init_media()::pjmedia_endpt_create()", &status);

    /* Must register codecs to be supported */

    status = pjmedia_codec_g711_init(g.media_endpt);
    if (status != PJ_SUCCESS)
        emergency_exit ("init_media()::pjmedia_codec_g711_init()", &status);

    status = pjmedia_null_port_create (g.pool, 8000, 1, 160, 16, &g.conf_null_port);
    if (status != PJ_SUCCESS)
        emergency_exit ("init_media()::pjmedia_null_port_create()", &status);
    
    status = pjmedia_conf_create (g.pool, 30, 8000, 1, 160, 16, PJMEDIA_CONF_NO_DEVICE, &g.conf);
    if (status != PJ_SUCCESS)
        emergency_exit ("init_media()::pjmedia_conf_create()", &status);

    pjmedia_port *conf_p0 = pjmedia_conf_get_master_port (g.conf);
    
    status = pjmedia_master_port_create (g.pool, g.conf_null_port, conf_p0, 0, &g.conf_mp);
    if (status != PJ_SUCCESS)
    {
        emergency_exit ("init_media() :: Error creating master port for conf bridge", &status);
    }

    status = pjmedia_master_port_start (g.conf_mp);
    if (status != PJ_SUCCESS)
    {
        emergency_exit ("init_media() :: Error starting master port for conf bridge", &status);
    }

    


}

void init_juncs ()
{
    
    int dis_count=0;
    const char *THIS_FUNCTION = "init_juncs()";
    /////
    pj_status_t status;
    pj_uint16_t rtp_port_in = (pj_uint16_t)(g.rtp_port_in & 0xFFFE);
    pj_uint16_t rtp_port_out = (pj_uint16_t)(g.rtp_port_out & 0xFFFE);

    for (int current_junc=0; current_junc<10; current_junc++)
    {
        junction_t *j = &g.junctions[current_junc];
        pj_status_t status;
        j->index = current_junc;
        //printf ("\n\n\n%d\n\n\n", current_junc);
        j->state = READY;

        

        nullize_leg (&j->in_leg);
        j->in_leg.type = IN;
        j->in_leg.reverse = &j->out_leg;
        j->in_leg.junction_index = j->index;
        

        nullize_leg (&j->out_leg);
        j->out_leg.type = OUT;
        j->out_leg.reverse = &j->in_leg;
        j->out_leg.junction_index = j->index;
        
        for (int retry=0; retry<=1500; retry++)
        {
            char name[22];
            sprintf (name, "in leg in j#%d", current_junc);
            status = pjmedia_transport_udp_create2 (g.media_endpt, name, &g.local_addr_in, rtp_port_in++, 0, &j->in_leg.media_transport);
            if (PJ_SUCCESS == status)
            {
                PJ_LOG (5, (APPNAME, "Media socket assigned to %s:%d (junc#%d/IN)", g.local_addr_in.ptr, rtp_port_in-1, current_junc));
                break;
            }
            else
            {
                //pj_perror (5, THIS_FUNCTION, status, "pjmedia_transport_udp_create2() with %s:%u", g.local_addr_in.ptr, rtp_port_in);
            }
            
        }

        if (j->state == DISABLED)
        {
            pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pjmedia_transport_udp_create2() for IN leg in junc#%d", j->index);
            continue;
        }
            
        for (int retry=0; retry<=1500; retry++)
        {
            char name[22];
            sprintf (name, "out leg in j#%d", current_junc);
            status = pjmedia_transport_udp_create2 (g.media_endpt, name, &g.local_addr_out, rtp_port_out++, 0, &j->out_leg.media_transport);
            if (PJ_SUCCESS == status)
            {
                PJ_LOG (5, (APPNAME, "Media socket assigned to %s:%d (junc#%d/IN)", g.local_addr_out.ptr, rtp_port_out-1, current_junc));
                break;
            }
            else
            {
                //pj_perror (5, THIS_FUNCTION, status, "pjmedia_transport_udp_create2() with %s:%u", g.local_addr_out.ptr, rtp_port_out);
            }
            

        }

        /*if (j->state == DISABLED)
        {
            pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pjmedia_transport_udp_create2() for OUT leg in junc#%d", j->index);
            status = pjmedia_transport_close (j->in_leg.media_transport);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, THIS_FUNCTION, status, "pjmedia_transport_close() for OUT leg in junc#%d", j->index);
                halt ("init_juncs()::pjmedia_transport_close()");
            }
            continue;
        } */
        
        status = pj_mutex_create (g.pool, "mutex", PJ_MUTEX_SIMPLE, &j->mutex);
        if (status != PJ_SUCCESS)
        {
            pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pj_mutex_create for junc#%d", j->index);
            /*j->state = DISABLED;

            status = pjmedia_transport_close (j->in_leg.media_transport);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pjmedia_transport_close() for IN leg in junc#%d", j->index);
                halt ("init_juncs()::pjmedia_transport_close()");
            }

            status = pjmedia_transport_close (j->out_leg.media_transport);
            if (status != PJ_SUCCESS)
            {
                pj_perror (5, THIS_FUNCTION, status, PJ_LOG_ERROR"pjmedia_transport_close() for out leg in junc#%d", j->index);
                halt ("init_juncs()::pjmedia_transport_close()");
            }
            continue;*/
        }
        //pjmedia_transport_media_start (j->in_leg.media_transport, 0, 0, 0, 0);
        //pjmedia_transport_media_start (j->out_leg.media_transport, 0, 0, 0, 0); 
    }   
}

void init_exits ()
{
    pj_status_t status;
    
    status = pj_mutex_create (g.pool, "exit mutex", PJ_MUTEX_SIMPLE, &g.exit_mutex);
    if (status != PJ_SUCCESS)
        halt ("init_utils()::pj_mutex_create()");

}
