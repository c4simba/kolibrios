
/*
 * SHELL <-> console-application protocol over shared memory.
 *
 * The shared memory segment is organised as a header followed by a byte
 * oriented ring buffer:
 *
 *   +0   write_ptr   offset of the next byte the CLIENT will write   (client owns)
 *   +4   read_ptr    offset of the next byte the SHELL will read     (shell owns)
 *   +8   resp_ready  set to 1 by the shell when a reply is available
 *                    (cleared to 0 by the client before a reply request)
 *   +12  resp_len    length of the reply payload in resp[]
 *   +16  resp[]      reply payload (shell -> client), SC_RESP_MAX bytes
 *   ...  ring[]       command stream (client -> shell), starts at SC_DATA_OFF
 *
 * The command stream is a sequence of variable length frames:
 *
 *   [cmd:1][len_lo:1][len_hi:1][payload:len]
 *
 * Frames are written contiguously in the ring and wrap byte-by-byte at the
 * end of the ring; a single frame may therefore straddle the wrap point.
 * The client publishes write_ptr only after a whole frame has been written,
 * so whenever read_ptr != write_ptr the shell is guaranteed to see complete
 * frames.  The shell drains every pending frame on each poll and advances
 * read_ptr, which lets the client queue many output frames without waiting
 * for each one (the old protocol blocked on every single message).
 *
 * Commands that return data (SC_GETC, SC_GETS, SC_PID) and the acknowledged
 * commands (SC_PING, SC_EXIT) use the resp_ready / resp[] slot: the client
 * clears resp_ready, queues the request frame, then waits for resp_ready.
 * Such requests are issued strictly one at a time, so a single reply slot is
 * enough.
 */

#define SC_OK		0
#define SC_EXIT		1
#define SC_PUTC		2
#define SC_PUTS		3
#define SC_GETC		4
#define SC_GETS		5
#define SC_CLS		6
#define SC_PID		7
#define SC_PING		8

/* ---- ring buffer layout ------------------------------------------------ */

/* size of the shared memory segment created by the client */
#define SC_SHM_SIZE	(1024 * 16)

/* size of the shell -> client reply slot */
#define SC_RESP_MAX	1024

/* header field offsets (bytes) */
#define SC_OFF_WRITE	0
#define SC_OFF_READ	4
#define SC_OFF_RESP	8
#define SC_OFF_RLEN	12
#define SC_OFF_RDATA	16

/* command stream (ring) starts right after the header + reply slot */
#define SC_DATA_OFF	(SC_OFF_RDATA + SC_RESP_MAX)	/* 1040 */

/* frame header: cmd byte + 2 length bytes */
#define SC_FRAME_HDR	3

#ifndef __ASSEMBLER__

typedef struct
{
	volatile unsigned	write_ptr;		/* +0  */
	volatile unsigned	read_ptr;		/* +4  */
	volatile unsigned	resp_ready;		/* +8  */
	volatile unsigned	resp_len;		/* +12 */
	unsigned char		resp[SC_RESP_MAX];	/* +16 */
	/* ring buffer data follows at offset SC_DATA_OFF == sizeof(sc_shm_t) */
} sc_shm_t;

#endif /* __ASSEMBLER__ */
