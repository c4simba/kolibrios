#include <shell_api.h>
#include <sys/ksys.h>

void shell_exit()
{
    if(__shell_is_init){
        // wait until the shell has drained every queued frame and acknowledged
        __shell_request(SHELL_EXIT, NULL, 0);
        _ksys_shm_close(__shell_shm_name);
    }
}
