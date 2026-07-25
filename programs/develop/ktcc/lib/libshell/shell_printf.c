#include <shell_api.h>
#include <stdio.h>
#include <string.h>

static char __shell_pbuf[SHELL_RING_SIZE];

void shell_printf(const char *format,...)
{
    va_list ap;
    __shell_init();
    va_start (ap, format);
    vsnprintf(__shell_pbuf, sizeof(__shell_pbuf), format, ap);
    va_end(ap);
    __shell_send(SHELL_PUTS, __shell_pbuf, strlen(__shell_pbuf) + 1);
}
