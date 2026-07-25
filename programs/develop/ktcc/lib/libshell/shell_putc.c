#include <shell_api.h>

void shell_putc(char c)
{
    __shell_init();
    __shell_send(SHELL_PUTC, &c, 1);
}
