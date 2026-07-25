#include <shell_api.h>
#include <string.h>

void shell_puts(const char *str)
{
    __shell_init();
    // send the string together with its terminating '\0' so the shell can
    // print the frame payload directly
    __shell_send(SHELL_PUTS, str, strlen(str) + 1);
}
