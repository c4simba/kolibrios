#include <shell_api.h>
#include <string.h>

unsigned shell_get_pid()
{
    shell_shm_t *h;
    unsigned pid;
    __shell_init();
    h = (shell_shm_t*)__shell_shm;
    __shell_request(SHELL_PID, NULL, 0);
    memcpy(&pid, (void*)h->resp, sizeof(unsigned));
    return pid;
}
