#include <sys/ksys.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <shell_api.h>

char app_name[13];
char __shell_shm_name[32];
char*__shell_shm=NULL;
int __shell_is_init=0;

int __shell_shm_init()
{
    __shell_is_init=1;
    ksys_thread_t *proc_info = (ksys_thread_t*)malloc(sizeof(ksys_thread_t));
    if(proc_info == NULL){
        return -1;
    }
    unsigned PID;
    int result;

    _ksys_thread_info(proc_info, -1);
    PID = proc_info->pid;
    strncpy(app_name, proc_info->name, 12);
    free(proc_info);

    itoa(PID, __shell_shm_name);
    strcat(__shell_shm_name, "-SHELL");
    result = _ksys_shm_open(__shell_shm_name,  KSYS_SHM_OPEN_ALWAYS | KSYS_SHM_WRITE, SHELL_SHM_MAX, &__shell_shm);
    if(__shell_shm != NULL){
        // start with an empty ring and no pending reply
        shell_shm_t *h = (shell_shm_t*)__shell_shm;
        h->write_ptr = 0;
        h->read_ptr = 0;
        h->resp_ready = 0;
        h->resp_len = 0;
    }
    return result;
}

void __shell_init()
{
    if(!__shell_is_init){
        if(__shell_shm_init()){
        debug_printf("%s: shell problems detected!\n", app_name);
        _ksys_exit();
        }

        if(!shell_ping()){
        debug_printf("%s: no shell found!\n", app_name);
        _ksys_exit();
        }
    }
}

// free space left in the ring (one byte is kept unused to tell full from empty)
static unsigned __shell_ring_free()
{
    shell_shm_t *h = (shell_shm_t*)__shell_shm;
    unsigned used = (h->write_ptr - h->read_ptr + SHELL_RING_SIZE) % SHELL_RING_SIZE;
    return SHELL_RING_SIZE - 1 - used;
}

// queue one [cmd][len][payload] frame; blocks only while the ring is full
void __shell_send(unsigned char cmd, const void *payload, unsigned len)
{
    shell_shm_t *h = (shell_shm_t*)__shell_shm;
    unsigned char *ring = (unsigned char*)__shell_shm + SHELL_DATA_OFF;
    const unsigned char *p = (const unsigned char*)payload;
    unsigned total = SHELL_FRAME_HDR + len;
    unsigned wp;
    unsigned i;

    if (total > SHELL_RING_SIZE - 1) {           // never fits: truncate payload
        len = SHELL_RING_SIZE - 1 - SHELL_FRAME_HDR;
        total = SHELL_FRAME_HDR + len;
    }

    while (__shell_ring_free() < total)          // back off until there is room
        _ksys_delay(1);

    wp = h->write_ptr;

    ring[wp] = cmd;                 wp++; if (wp == SHELL_RING_SIZE) wp = 0;
    ring[wp] = (unsigned char)(len & 0xff);        wp++; if (wp == SHELL_RING_SIZE) wp = 0;
    ring[wp] = (unsigned char)((len >> 8) & 0xff); wp++; if (wp == SHELL_RING_SIZE) wp = 0;
    for (i = 0; i < len; i++) {
        ring[wp] = p[i];            wp++; if (wp == SHELL_RING_SIZE) wp = 0;
    }

    h->write_ptr = wp;              // publish the whole frame at once
}

// queue a frame and wait for the shell to place a reply in resp[]
void __shell_request(unsigned char cmd, const void *payload, unsigned len)
{
    shell_shm_t *h = (shell_shm_t*)__shell_shm;
    h->resp_ready = 0;
    __shell_send(cmd, payload, len);
    while (!h->resp_ready)
        _ksys_delay(2);
}
