
#include "system/kolibri.h"
#include "system/string.h"

#include "program_console.h"

/*
 * Example console application for SHELL.
 *
 * Communication goes through a shared memory ring buffer (see
 * program_console.h): output commands (puts/putc/cls) are simply queued into
 * the ring and only block if the ring is full, while commands that need an
 * answer (gets/getc) queue a request and wait for the reply slot.
 */

char		*shm;		// base of the shared memory segment
char		name[32];	// SHM name, also needed to close it


static unsigned sc_ring_free(void)
{
	sc_shm_t *h = (sc_shm_t *) shm;
	unsigned ring_size = SC_SHM_SIZE - SC_DATA_OFF;
	unsigned used = (h->write_ptr - h->read_ptr + ring_size) % ring_size;
	return ring_size - 1 - used;
}


// queue one [cmd][len][payload] frame; blocks only while the ring is full
static void sc_send(unsigned char cmd, const void *payload, unsigned len)
{
	sc_shm_t		*h = (sc_shm_t *) shm;
	unsigned char		*ring = (unsigned char *) shm + SC_DATA_OFF;
	const unsigned char	*p = (const unsigned char *) payload;
	unsigned		ring_size = SC_SHM_SIZE - SC_DATA_OFF;
	unsigned		total = SC_FRAME_HDR + len;
	unsigned		wp, i;

	if (total > ring_size - 1) {
		len = ring_size - 1 - SC_FRAME_HDR;
		total = SC_FRAME_HDR + len;
	}

	while (sc_ring_free() < total)
		kol_sleep(1);

	wp = h->write_ptr;

	ring[wp] = cmd;                                wp++; if (wp == ring_size) wp = 0;
	ring[wp] = (unsigned char)(len & 0xff);        wp++; if (wp == ring_size) wp = 0;
	ring[wp] = (unsigned char)((len >> 8) & 0xff); wp++; if (wp == ring_size) wp = 0;
	for (i = 0; i < len; i++) {
		ring[wp] = p[i];                       wp++; if (wp == ring_size) wp = 0;
	}

	h->write_ptr = wp;	// publish the whole frame at once
}


// queue a request frame and wait for the shell to fill the reply slot
static void sc_request(unsigned char cmd, const void *payload, unsigned len)
{
	sc_shm_t *h = (sc_shm_t *) shm;
	h->resp_ready = 0;
	sc_send(cmd, payload, len);
	while (!h->resp_ready)
		kol_sleep(2);
}


int sc_init() // create the shared buffer
{
	char		*buf1k;
	unsigned	PID;
	int		result;
	sc_shm_t	*h;

	buf1k = malloc(1024);
	if (NULL == buf1k)
		return -1;

	kol_process_info(-1, buf1k); // read our own (-1) process info
	PID = *(buf1k+30);
	free(buf1k);

	itoa(PID, name); // build the buffer name from the PID
	strcat(name, "-SHELL");

	shm = NULL;
	result = kol_buffer_open(name, SHM_OPEN_ALWAYS | SHM_WRITE, SC_SHM_SIZE, &shm);
	if (NULL == shm)
		return result;

	// start with an empty ring and no pending reply
	h = (sc_shm_t *) shm;
	h->write_ptr = 0;
	h->read_ptr = 0;
	h->resp_ready = 0;
	h->resp_len = 0;

	return result;
}


void sc_puts(char *str)
{
	sc_send(SC_PUTS, str, strlen(str) + 1);	// include the trailing '\0'
}


void sc_exit()
{
	sc_request(SC_EXIT, NULL, 0);	// wait until the shell drained everything
	kol_buffer_close(name);
}


void sc_gets(char *str)
{
	sc_shm_t *h = (sc_shm_t *) shm;
	unsigned max = 256;
	sc_request(SC_GETS, &max, sizeof(max));
	strcpy(str, (char *) h->resp);
}


char sc_getc()
{
	sc_shm_t *h = (sc_shm_t *) shm;
	sc_request(SC_GETC, NULL, 0);
	return (char) h->resp[0];
}


void sc_putc(char c)
{
	sc_send(SC_PUTC, &c, 1);
}


void sc_cls()
{
	sc_send(SC_CLS, NULL, 0);
}




void kol_main()
{

char string[256];

sc_init();

sc_cls();
sc_puts("This is a test console application for Shell\n\r");
sc_puts("Type a string (255 symbols max): ");
sc_gets(string);
sc_puts("You typed:\n\r");
sc_puts(string);
sc_puts("Press any key: ");
string[0] = sc_getc();
sc_puts("\n\rYou pressed: ");
sc_putc(string[0]);

sc_exit();



}
