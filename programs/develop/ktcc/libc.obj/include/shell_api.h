#ifndef _SHELL_API_H_
#define _SHELL_API_H_

#include <sys/ksys.h>

/*
 * Protocol between a console application and SHELL over shared memory.
 *
 * The segment is a header followed by a byte oriented ring buffer that carries
 * a stream of variable length command frames from the application to the shell:
 *
 *      [cmd:1][len_lo:1][len_hi:1][payload:len]
 *
 * The application appends frames and only publishes write_ptr once a whole
 * frame is written, so it can queue many output frames without waiting for
 * each of them.  The shell drains all pending frames on every poll and
 * advances read_ptr.  Commands that return data or need an acknowledge use the
 * resp_ready / resp[] reply slot and are issued one at a time.
 */

#define SHELL_OK   0
#define SHELL_EXIT 1
#define SHELL_PUTC 2
#define SHELL_PUTS 3
#define SHELL_GETC 4
#define SHELL_GETS 5
#define SHELL_CLS  6
#define SHELL_PID  7
#define SHELL_PING 8

/* size of the shared memory segment created by the application */
#define SHELL_SHM_MAX (1024 * 16)

/* size of the shell -> application reply slot */
#define SHELL_RESP_MAX 1024

/* ring buffer data starts right after the header + reply slot */
#define SHELL_DATA_OFF (16 + SHELL_RESP_MAX)          /* 1040 */

/* usable ring buffer size for the command stream */
#define SHELL_RING_SIZE (SHELL_SHM_MAX - SHELL_DATA_OFF)

/* frame header: cmd byte + 2 length bytes */
#define SHELL_FRAME_HDR 3

typedef struct
{
    volatile unsigned write_ptr;              /* +0  application owns */
    volatile unsigned read_ptr;               /* +4  shell owns       */
    volatile unsigned resp_ready;             /* +8  reply flag       */
    volatile unsigned resp_len;               /* +12 reply length     */
    unsigned char     resp[SHELL_RESP_MAX];   /* +16 reply payload    */
    /* ring buffer data follows at offset SHELL_DATA_OFF */
} shell_shm_t;

extern char __shell_shm_name[32];
extern char* __shell_shm;
extern int __shell_is_init;
extern void __shell_init();

/* queue an output frame (blocks only while the ring is full) */
extern void __shell_send(unsigned char cmd, const void* payload, unsigned len);
/* queue a request frame and wait for the shell to fill the reply slot */
extern void __shell_request(unsigned char cmd, const void* payload, unsigned len);

extern int shell_ping();
extern unsigned shell_get_pid();
extern void shell_exit();

extern char shell_getc();
extern void shell_gets(char* str, int n);

extern void shell_putc(char c);
extern void shell_puts(const char* str);
extern void shell_printf(const char* format, ...);

extern void shell_cls();
#endif
