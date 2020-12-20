#include "exits.h"
extern struct global_var g;

void emergency_exit (char *sender, pj_status_t *status)
{

    static const char *THIS_FUNCTION = "emergency_exit()";
    
    printf ("\n\n\nEMERGENCY EXIT CALLED FROM %s\n\n\n", sender==NULL ? "unknown" : sender);

    if (status != NULL)
        pj_perror (5, THIS_FUNCTION, *status, sender);
    
    if (pj_mutex_trylock (g.exit_mutex) != PJ_SUCCESS)
        return;

    for (int i=0; i<10; i++)
        destroy_junction (&g.junctions[i]);
    pj_pool_release (g.pool);
    pj_caching_pool_destroy (&g.cp);
    exit (2);
}

void halt (char *sender)
{
    printf ("\n\n\nHALT CALLED FROM %s\n\n\n", sender==NULL ? "unknown" : sender);
    exit (1);
}
