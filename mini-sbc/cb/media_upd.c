#include "media_upd.h"
extern struct global_var g;
void on_media_update (pjsip_inv_session *inv, pj_status_t status)
{
    const char THIS_FUNCTION[] = "on_media_update()";

    if (inv == NULL)
    {
        PJ_LOG (5, (THIS_FUNCTION, "Gotten empty inv pointer"));
        return;
    }

    leg_t *l = (leg_t*) inv->mod_data[g.mod_app.id];

    if (l == NULL)
        halt ("on_media_upd()");

    char FULL_INFO[64];
    sprintf (FULL_INFO, "%s at %s leg in junc#%d", THIS_FUNCTION, l->type == IN ? "IN" : "OUT", l->junction_index);

    pjmedia_stream_info stream_info;
    const pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *remote_sdp;
    
    PJ_LOG(5, (FULL_INFO, "Entered"));

    if (status != PJ_SUCCESS) 
    {
        pj_perror (5, FULL_INFO, status, "argument 2 (status) invalid");
        return;
    }
    

    /* Get local and remote SDP.
     * We need both SDPs to create a media session.
     */
    status = pjmedia_sdp_neg_get_active_local(inv->neg, &local_sdp);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, FULL_INFO, status, "pjmedia_sdp_neg_get_active_local()");
            return;
    } 

    status = pjmedia_sdp_neg_get_active_remote(inv->neg, &remote_sdp);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, FULL_INFO, status, "pjmedia_sdp_neg_get_active_remote()");
            return;
    } 
    
    status = pjmedia_transport_media_create (l->media_transport, l->current.inv->pool, 0, remote_sdp, 0);
    if (status != PJ_SUCCESS)
        halt ("pjmedia_transport_media_create");

    status = pjmedia_transport_media_start (l->media_transport, l->current.inv->pool, local_sdp, remote_sdp, 0);
    if (status != PJ_SUCCESS)
        halt ("pjmedia_transport_media_start");
    // Create stream info based on the media audio SDP. 
    status = pjmedia_stream_info_from_sdp(&stream_info, inv->pool, g.media_endpt, local_sdp, remote_sdp, 0);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, FULL_INFO, status, "pjmedia_stream_info_from_sdp()");
            return;
    } 

    

    /* Create new audio media stream, passing the stream info, and also the
     * media socket that we created earlier.
     */
    status = pjmedia_stream_create(g.media_endpt, inv->pool, &stream_info, l->media_transport, NULL, &l->current.stream);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, FULL_INFO, status, "pjmedia_stream_create()");
            return;
    } 

    // Start the audio stream 
    
	
    
    
    status = pjmedia_stream_get_port(l->current.stream, &l->current.stream_port);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, FULL_INFO, status, "pjmedia_stream_get_port()");
            return;
    }
    
    status = pjmedia_conf_add_port (g.conf, l->current.inv->pool, l->current.stream_port, NULL, &l->current.stream_conf_id); // CATCH
    if (status != PJ_SUCCESS)
        halt ("pjmedia_conf_add_port");
}