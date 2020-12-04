#include "console.h"
extern struct global_var g;

int console_thread (void *p)
{
    char c;

    while ( (c = getchar()) != 'q')
    {
        switch (c)
        {
            case 'p':
                if (!g.pause)
                {
                    printf ("\n\n\nPRESS 'p' KEY AGAIN TO CONTINUE...\n\n\n");
                    g.pause = 1;
                }
                else
                {
                    printf ("\n\n\nCONTINUING...\n\n\n");
                    g.pause = 0; 
                }
            break;
                
                
        }
    }
    g.to_quit = 1;
    return 0;
}