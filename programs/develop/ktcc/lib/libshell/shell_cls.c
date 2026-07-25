#include <shell_api.h>

void shell_cls()
{
    __shell_init();
    __shell_send(SHELL_CLS, NULL, 0);
}
