#include <shell_api.h>
#include <string.h>

void shell_gets(char *str, int n)
{
    shell_shm_t *h;
    unsigned max;
    __shell_init();
    h = (shell_shm_t*)__shell_shm;
    // tell the shell how many characters we can accept
    max = (unsigned)n;
    __shell_request(SHELL_GETS, &max, sizeof(max));
    strncpy(str, (char*)h->resp, n);
}
