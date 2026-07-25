#include "shell_api.h"

char shell_getc()
{
    shell_shm_t *h;
    __shell_init();
    h = (shell_shm_t*)__shell_shm;
    __shell_request(SHELL_GETC, NULL, 0);
    return (char)h->resp[0];
}
