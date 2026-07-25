
#include "../program_console.h"

/* How the console renders (this matters for speed):
 *   - Writing text (con_write_string) is cheap: it just fills the console's
 *     character buffer. The expensive part is the actual window repaint, done
 *     on the library's own thread, which only happens when this thread yields.
 *   - The shell's own commands (e.g. `help`) are fast because they write every
 *     line to the buffer and repaint ONCE, when they return to the prompt.
 *
 * So we do the same for a console app's output: accumulate a whole output burst
 * into `acc` and repaint only once, when the burst ends (the ring stays empty
 * through a short debounce) or right before a blocking input. Repainting per
 * frame - or per drain pass - is what made bulk output crawl. */

#define SC_ACC_CAP   (256 * 1024)   /* how much output we coalesce before a repaint */
#define SC_IDLE_SPINS 64            /* debounce: yields to wait for more output before repainting */


int program_console(int pid) {
    char name[32];
    char *shm;
    sc_shm_t *hdr;
    unsigned char *ring;
    unsigned ring_size;
    char *frame;        // contiguous reassembly buffer for one frame payload
    char *acc;          // coalesced output text, painted with ONE con_write_string
    unsigned accn;
    char *buf1k;
    unsigned rp, wp;
    int result;
    int i;
    int is_end;

    itoa(pid, name);
    strcat(name, "-SHELL");

    shm = NULL;

    for (i = 0; i < 30;  i++) {
        result = kol_buffer_open(name, SHM_OPEN | SHM_WRITE, 0, &shm);
        if (shm != NULL)
            break;

        kol_sleep(2);
    }

    if (shm == NULL)
        return 0;

    if ((unsigned)result <= SC_DATA_OFF) // buffer too small to hold a ring
        return 0;

    hdr = (sc_shm_t *) shm;
    ring = (unsigned char *) shm + SC_DATA_OFF;
    ring_size = (unsigned)result - SC_DATA_OFF;

    frame = malloc(ring_size);
    acc   = malloc(SC_ACC_CAP);
    if (frame == NULL || acc == NULL) {
        if (frame) free(frame);
        if (acc)   free(acc);
        return 0;
    }
    accn = 0;

    /* SC_WRITE: push accumulated text into the console buffer WITHOUT forcing a
       repaint. SC_PAINT: same, then yield so the console thread repaints now. */
    #define SC_WRITE() do { if (accn) { con_write_string(acc, accn); accn = 0; } } while (0)
    #define SC_PAINT() do { SC_WRITE(); kol_yield(); } while (0)

    rp = hdr->read_ptr;
    is_end = 0;

    for (;;) {
        wp = hdr->write_ptr;

        if (rp == wp) {                 // ring momentarily empty
            if (accn) {
                /* Debounce: the producer may just be between frames. Give it a
                   few yields to enqueue more so a whole burst coalesces into a
                   single repaint instead of one repaint per chunk. */
                int spin;
                for (spin = 0; spin < SC_IDLE_SPINS && hdr->write_ptr == rp; spin++)
                    kol_yield();
                if (hdr->write_ptr != rp)
                    continue;           // more output arrived - keep accumulating
                SC_PAINT();             // burst ended - paint it all at once
            }
            kol_sleep(1);               // idle poll
            continue;
        }

        // drain every complete frame the client has published so far
        while (rp != wp) {
            unsigned char command;
            unsigned len, j;

            command = ring[rp];  rp++;  if (rp == ring_size) rp = 0;

            len  =            ring[rp];  rp++;  if (rp == ring_size) rp = 0;
            len |= (unsigned) ring[rp] << 8;  rp++;  if (rp == ring_size) rp = 0;

            if (len >= ring_size)   // defensive clamp against a bad frame
                len = ring_size - 1;

            for (j = 0; j < len; j++) {
                frame[j] = ring[rp];  rp++;  if (rp == ring_size) rp = 0;
            }

            // publish consumption before any (possibly blocking) processing,
            // so the client can keep reusing ring space while we work
            hdr->read_ptr = rp;

            switch (command) {
                case SC_PUTS: {
                    unsigned plen = len ? len - 1 : 0;   // frame has a trailing '\0'
                    if (accn + plen > SC_ACC_CAP)        // acc full: push to buffer, keep going
                        SC_WRITE();
                    memcpy(acc + accn, frame, plen);
                    accn += plen;
                    break;
                }

                case SC_PUTC:
                    if (accn + 1 > SC_ACC_CAP)
                        SC_WRITE();
                    acc[accn++] = frame[0];
                    break;

                case SC_CLS:
                    SC_PAINT();
                    con_cls();
                    break;

                case SC_GETC:
                    SC_PAINT();                          // show queued text + prompt before blocking
                    hdr->resp[0] = (char) getch();
                    hdr->resp_len = 1;
                    hdr->resp_ready = 1;
                    break;

                case SC_GETS: {
                    unsigned n = SC_RESP_MAX;
                    if (len >= sizeof(unsigned))
                        memcpy(&n, frame, sizeof(unsigned));
                    if (n == 0 || n > SC_RESP_MAX)
                        n = SC_RESP_MAX;
                    SC_PAINT();                          // show queued text + prompt before blocking
                    gets((char *) hdr->resp, n - 1);
                    hdr->resp_len = strlen((char *) hdr->resp);
                    hdr->resp_ready = 1;
                    break;
                }

                case SC_PID:
                    buf1k = malloc(1024);
                    kol_process_info(-1, buf1k);
                    memcpy(hdr->resp, buf1k + 30, sizeof(unsigned));
                    hdr->resp_len = sizeof(unsigned);
                    hdr->resp_ready = 1;
                    free(buf1k);
                    break;

                case SC_PING:
                    hdr->resp_ready = 1;
                    break;

                case SC_EXIT:
                    SC_PAINT();
                    hdr->resp_ready = 1;
                    is_end = 1;
                    break;

                default:
                    SC_PAINT();
                    printf (CON_APP_ERROR);
                    free(frame);
                    free(acc);
                    return 0;
            };

            if (is_end)
                break;
        } // drain end

        if (is_end) {
            SC_PAINT();
            printf("\n\r");
            free(frame);
            free(acc);
            return 1;
        }
    } // for end

    #undef SC_WRITE
    #undef SC_PAINT

    free(frame);
    free(acc);
    return 9;
}
