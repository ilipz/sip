#include "media_upd.h"
extern struct global_var g;
void on_media_update (pjsip_inv_session *inv, pj_status_t status)
{
    const char THIS_FUNCTION[] = "call_on_media_update()";
    pjmedia_stream_info stream_info;
    const pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *remote_sdp;

    
    

    /*int index = get_slot_by_inv (inv);
    if (index == -1)
    {
        PJ_LOG (5, (THIS_FUNCTION, "error in get_slot_by_inv"));
        emergency_exit ();
    }
    
    PJ_LOG (5, (THIS_FUNCTION, "called for slot #%d", index)); */
    
    leg_t *tmp = inv->mod_data[g.mod_app.id];
    printf ("\n\n\n MEDIA UPD ENTERED %s\n\n\n", tmp->type == OUT ? "OUT LEG" : "IN LEG");
    if (status != PJ_SUCCESS) 
    {
        pj_perror (5, THIS_FUNCTION, status, "argument 2 (status) invalid");
        emergency_exit ();
    }
    

    /* Get local and remote SDP.
     * We need both SDPs to create a media session.
     */
    status = pjmedia_sdp_neg_get_active_local(inv->neg, &local_sdp);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_sdp_neg_get_active_local()");
            emergency_exit ();
    } 

    status = pjmedia_sdp_neg_get_active_remote(inv->neg, &remote_sdp);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_sdp_neg_get_active_remote()");
            emergency_exit ();
    } 

    // Create stream info based on the media audio SDP. 
    status = pjmedia_stream_info_from_sdp(&stream_info, inv->pool,
					  g.media_endpt,
					  local_sdp, remote_sdp, 0);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_stream_info_from_sdp()");
            emergency_exit ();
    } 

    

    /* Create new audio media stream, passing the stream info, and also the
     * media socket that we created earlier.
     */
    status = pjmedia_stream_create(g.media_endpt, inv->pool, &stream_info,
				   tmp->media_transport, NULL, &tmp->current.stream);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_stream_create()");
            emergency_exit ();
    } 

    // Start the audio stream 
    status = pjmedia_stream_start(tmp->current.stream);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_stream_start()");
            emergency_exit ();
    } 
	
    
    
    status = pjmedia_stream_get_port(tmp->current.stream, &tmp->current.stream_port);
    if (PJ_SUCCESS != status)
    {
            pj_perror (5, THIS_FUNCTION, status, "pjmedia_stream_get_port()");
            emergency_exit ();
    }

    tmp->current.sdp_neg_done = PJ_TRUE;
    
     

	
    
    
    

    
    
    //PJ_LOG (5, (THIS_FUNCTION, "exited for leg #%d", tmp->index));
    


    // Done with media. 
}