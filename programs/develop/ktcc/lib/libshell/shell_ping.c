#include <shell_api.h>
#include <sys/ksys.h>

int shell_ping()
{
    shell_shm_t *h;
    int i;
    __shell_init();
    h = (shell_shm_t*)__shell_shm;

    // bounded wait: unlike a normal request we must not block forever, this is
    // also used to detect whether a shell is present at all
    h->resp_ready = 0;
    __shell_send(SHELL_PING, NULL, 0);
    for (i = 0; i < 20; i++) {
        if (h->resp_ready)
            return 1;
        _ksys_delay(1);
    }
    return 0;
}
