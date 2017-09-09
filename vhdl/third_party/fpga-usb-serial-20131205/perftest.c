/*
 * Test performance of USB serial link.
 *
 * To test performance through CDC-ACM driver:
 *   perftest -t /dev/ttyACMn
 *
 * To test performance through libusb:
 *   perftest -d vendorid:productid
 *   perftest -s bus:devnum
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>
#include <libusb-1.0/libusb.h>


#define BUFSIZE		65536
#define BURSTCMDS	64
#define BURSTCNT	1000
#define BLASTRXSIZE	(64*1024*1024)
#define BLASTTXSIZE	64000000

#define USB_ENDPT_RX	0x81
#define USB_ENDPT_TX	0x01
#define USB_NUMURB	16
#define USB_URBSIZE	4096

#if BURSTCMDS * 256 > BUFSIZE
#error "Invalid parameters"
#endif

#if USB_NUMURB * USB_URBSIZE > BUFSIZE
#error "Invalid parameters"
#endif


static char buf[BUFSIZE];


static inline void timer_start(struct timeval *tv)
{
	gettimeofday(tv, NULL);
}


static inline double timer_elapsed(struct timeval *tv)
{
	struct timeval end;
	gettimeofday(&end, NULL);
	return end.tv_sec + 0.000001 * end.tv_usec -
	       tv->tv_sec - 0.000001 * tv->tv_usec;
}


/* Flush receive buffer from serial port. */
static size_t do_flush(int fd)
{
	struct timeval tv;
	fd_set rdfds;
	int r;
	ssize_t k;
	size_t n = 0;
	while (1) {
		tv.tv_sec = 0;
		tv.tv_usec = 250000;
		FD_ZERO(&rdfds);
		FD_SET(fd, &rdfds);
		r = select(fd + 1, &rdfds, NULL, NULL, &tv);
		if (r < 0) {
			perror("select");
			exit(1);
		}
		if (r == 0)
			break;
		k = read(fd, buf, sizeof(buf));
		if (k < 0) {
			perror("read");
			exit(1);
		}
		if (k == 0)
			break;
		n += k;
	}
	return n;
}


/* Read exactly count bytes from fd. */
static ssize_t do_read_all(int fd, void *buf, size_t count)
{
	size_t n = 0;
	while (n < count) {
		ssize_t k = read(fd, buf + n, count - n);
		if (k <= 0) {
			perror("read");
			exit(1);
		}
		n += k;
	}
	return n;
}


/* Write exactly count bytes to fd. */
static ssize_t do_write_all(int fd, const void *buf, size_t count)
{
	size_t n = 0;
	while (n < count) {
		ssize_t k = write(fd, buf + n, count - n);
		if (k <= 0) {
			perror("write");
			exit(1);
		}
		n += k;
	}
	return n;
}


/* Test performance through TTY device node. */
static void test_serial(const char *devname)
{
	int fd;
	struct termios tc;
	struct timeval tv;
	int i, j, k, bcnt;
	double elapsed;

	printf("Opening %s ... ", devname);

	/* Open TTY device. */
	fd = open(devname, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	/* Set TTY mode. */
	if (tcgetattr(fd, &tc) < 0) {
		perror("tcgetattr");
		exit(1);
	}
	tc.c_iflag &= ~(INLCR|IGNCR|ICRNL|IGNBRK|IUCLC|INPCK|ISTRIP|IXON|IXOFF|IXANY);
	tc.c_oflag &= ~OPOST;
	tc.c_cflag &= ~(CSIZE|CSTOPB|PARENB|PARODD|CRTSCTS);
	tc.c_cflag |= CS8 | CREAD | CLOCAL;
	tc.c_lflag &= ~(ICANON|ECHO|ECHOE|ECHOK|ECHONL|ISIG|IEXTEN);
	tc.c_cc[VMIN] = 1;
	tc.c_cc[VTIME] = 0;
	if (tcsetattr(fd, TCSANOW, &tc) < 0) {
		perror("tcsetattr");
		exit(1);
	}
	usleep(250000);

	printf("ok.\n");

	printf("Switching to binary mode ... ");
	do_write_all(fd, "\0\0\0\001", 4);
	k = do_flush(fd);
	printf("flushed %d bytes.\n", k);

	printf("Testing %d bursts of %d commands each ... \n", BURSTCNT, BURSTCMDS);
	usleep(100000);
	timer_start(&tv);
	for (i = 0; i < BURSTCNT; i++) {
		for (j = 0; j < BURSTCMDS; j++) {
			buf[2*j] = 250;
			buf[2*j+1] = i;
		}
		do_write_all(fd, buf, 2 * BURSTCMDS);
		do_read_all(fd, buf, 250 * BURSTCMDS);
		for (j = 0; j < BURSTCMDS; j++) {
			for (k = 0; k < 250; k++) {
				if ((unsigned char)buf[250*j+k] != (unsigned char)(i + k)) {
					fprintf(stderr, "data corrupted at j=%d\n", j);
					exit(1);
				}
			}
		}
	}
	elapsed = timer_elapsed(&tv);
	printf("  TX %8d + RX %8d in %6.3f s = %8.0f bytes/s\n",
	       2 * BURSTCNT * BURSTCMDS, 250 * BURSTCNT * BURSTCMDS,
	       elapsed, 252 * BURSTCNT * BURSTCMDS / elapsed);

	printf("Testing streaming RX %d bytes ... \n", BLASTRXSIZE);
	usleep(100000);
	timer_start(&tv);
	do_write_all(fd, "\0\0\0\006", 4);
	bcnt = 0;
	while (bcnt < BLASTRXSIZE) {
		unsigned char t;
		k = BLASTRXSIZE - bcnt;
		if (k > sizeof(buf))
			k = sizeof(buf);
		k = read(fd, buf, k);
		if (k < 0) {
			perror("read");
			exit(1);
		}
		t = bcnt % 253;
		for (j = 0; j < k; j++) {
			if ((unsigned char)buf[j] != t) {
				fprintf(stderr, "data corrupted at p=%d got=%u exp=%d\n", bcnt + j, (unsigned char)buf[j], t);
				exit(1);
			}
			t = (t < 252) ? (t + 1) : 0;
		}
		bcnt += k;
	}
	elapsed = timer_elapsed(&tv);
	printf("  RX %8d bytes in %6.3f s = %8.0f bytes/s\n",
	       bcnt, elapsed, bcnt / elapsed);
	k = do_flush(fd);
	if (k) {
		fprintf(stderr, "unexpected junk after RX blast\n");
		exit(1);
	}

	printf("Estimating loss-rate at 25 MHz RX ... \n");
	usleep(100000);
	do_write_all(fd, "\0\0\0\007", 4);
	k = do_flush(fd);
	printf("  lost %d out of %d bytes = %3.3f %%\n",
               BLASTRXSIZE - k, BLASTRXSIZE,
	       100.0 * (BLASTRXSIZE - k) / (double)BLASTRXSIZE);

	printf("Testing streaming TX %d bytes ... \n", BLASTTXSIZE);
	usleep(100000);
	timer_start(&tv);
	bcnt = 0;
	while (bcnt < BLASTTXSIZE) {
		k = BLASTTXSIZE - bcnt;
		if (k > sizeof(buf))
			k = sizeof(buf);
		memset(buf, 0, k);
		do_write_all(fd, buf, k);
		bcnt += k;
	}
	elapsed = timer_elapsed(&tv);
	printf("  TX %8d bytes in %6.3f s = %8.0f bytes/s\n",
	       bcnt, elapsed, bcnt / elapsed);

	do_flush(fd);
	close(fd);

	printf("Done.\n");
}


/* Print libusb error message and exit. */
static void usb_failed(const char *msg, int errorcode)
{
	fprintf(stderr, "Unexpected error in %s (%s)\n",
	        msg, libusb_error_name(errorcode));
	exit(1);
}


/* Flush receive channel from USB device. */
static int usb_flush(libusb_device_handle *handle)
{
	int r, n = 0, k;
	while (1) {
		r = libusb_bulk_transfer(handle, USB_ENDPT_RX, (unsigned char *)buf, USB_URBSIZE, &k, 250);
		if (r == LIBUSB_ERROR_TIMEOUT)
			break;
		if (r)
			usb_failed("usb_flush", r);
		n += k;
	}
	return n;
}


/* Synchronously write a string to the USB device. */
static void usb_write(libusb_device_handle *handle, const char *str, size_t n)
{
	int r, k;
	r = libusb_bulk_transfer(handle, USB_ENDPT_TX, (unsigned char*)str, n, &k, 0);
	if (r)
		usb_failed("usb_write", r);
}


/* Callback handler for streaming RX. */
static void usb_streamrx_cb(struct libusb_transfer *urb)
{
	int *bcnt;
	int t, j, k, r;

	bcnt = (int *)(urb->user_data);

	if (urb->status == LIBUSB_TRANSFER_CANCELLED) {
		bcnt[2]++;
		bcnt[3] = (bcnt[2] == USB_NUMURB);
		return;
	}

	if (urb->status != LIBUSB_TRANSFER_COMPLETED) {
		fprintf(stderr, "usb_streamrx_cb: URB failed\n");
		exit(1);
	}

	k = urb->actual_length;
	t = bcnt[0];
	t = t % 253;
	for (j = 0; j < k; j++) {
		if (urb->buffer[j] != t) {
			fprintf(stderr, "data corrupted\n");
			exit(1);
		}
		t = (t < 252) ? (t + 1) : 0;
	}
	bcnt[0] += k;

	if (bcnt[0] >= BLASTRXSIZE) {
		bcnt[2]++;
		bcnt[3] = 1;
		return;
	}

	libusb_fill_bulk_transfer(urb,
	                          urb->dev_handle, USB_ENDPT_RX,
		                  urb->buffer, urb->length,
			          usb_streamrx_cb, urb->user_data,
				  0);
	r = libusb_submit_transfer(urb);
	if (r)
		usb_failed("streamrx_cb_submit", r);
}


/* Callback handler for RX during loss estimation. */
static void usb_lossrx_cb(struct libusb_transfer *urb)
{
	int *bcnt;
	int r;

	bcnt = (int *)(urb->user_data);

	if (urb->status == LIBUSB_TRANSFER_CANCELLED ||
	    urb->status == LIBUSB_TRANSFER_TIMED_OUT) {
		bcnt[2]++;
		bcnt[3] = (bcnt[2] == USB_NUMURB);
		return;
	}

	if (urb->status != LIBUSB_TRANSFER_COMPLETED) {
		fprintf(stderr, "usb_lossrx_cb: URB failed\n");
		exit(1);
	}

	bcnt[0] += urb->actual_length;

	libusb_fill_bulk_transfer(urb,
	                          urb->dev_handle, USB_ENDPT_RX,
	                          urb->buffer, urb->length,
	                          usb_lossrx_cb, urb->user_data,
	                          1000);
	r = libusb_submit_transfer(urb);
	if (r)
		usb_failed("lossrx_cb_submit", r);
}


/* Callback handler for streaming TX. */
static void usb_streamtx_cb(struct libusb_transfer *urb)
{
	int *bcnt, k, r;

	bcnt = (int *)(urb->user_data);

	if (urb->status == LIBUSB_TRANSFER_CANCELLED) {
		bcnt[2]++;
		bcnt[3] = (bcnt[2] == USB_NUMURB);
		return;
	}

	if (urb->status != LIBUSB_TRANSFER_COMPLETED) {
		fprintf(stderr, "usb_streamtx_cb: URB failed\n");
		exit(1);
	}

	k = urb->actual_length;
	bcnt[0] += k;

	if (urb->actual_length != urb->length) {
		fprintf(stderr, "usb_streamtx_cb: length=%d actual_length=%d\n", urb->length, urb->actual_length);
		exit(1);
	}

	if (bcnt[1] >= BLASTTXSIZE) {
		bcnt[2]++;
		bcnt[3] = 1;
		return;
	}

	k = BLASTTXSIZE - bcnt[1];
	if (k > USB_URBSIZE)
		k = USB_URBSIZE;
	bcnt[1] += k;
	memset(urb->buffer, 0, k);
	libusb_fill_bulk_transfer(urb,
		                  urb->dev_handle, USB_ENDPT_TX,
			          urb->buffer, k,
				  usb_streamtx_cb, bcnt,
	                          0);
	r = libusb_submit_transfer(urb);
	if (r)
		usb_failed("streamtx_cb_submit", r);
}


/* Test performance through libusb, using specified device. */
static void test_libusb(int vendorid, int productid, int busnum, int devnum)
{
	libusb_context *ctx;
	libusb_device_handle *handle = NULL;
	libusb_device **devlist;
	struct libusb_device_descriptor desc;
	struct libusb_transfer *urb[USB_NUMURB];
	struct timeval tv;
	double elapsed;
	ssize_t ndev;
	int i, k, bcnt[4], r, config, kdrv;

	r = libusb_init(&ctx);
	if (r) {
		fprintf(stderr, "Can not initialize libusb.\n");
		exit(1);
	}
	libusb_set_debug(ctx, 2);

	if (vendorid != -1 && productid != -1) {
		handle = libusb_open_device_with_vid_pid(ctx, vendorid, productid);
		if (!handle) {
			fprintf(stderr, "Can not open USB device\n");
			exit(1);
		}
	} else {
		assert(busnum != -1 && devnum != -1);
		ndev = libusb_get_device_list(ctx, &devlist);
		if (ndev < 0)
			exit(1);
		for (i = 0; i < ndev; i++) {
			if (libusb_get_bus_number(devlist[i]) == busnum &&
			    libusb_get_device_address(devlist[i]) == devnum) {
				r = libusb_open(devlist[i], &handle);
				if (r)
					usb_failed("libusb_open", r);
				break;
			}
		}
		libusb_free_device_list(devlist, 1);
		if (!handle) {
			fprintf(stderr, "No device found with bus=%d dev=%d\n", busnum, devnum);
			exit(1);
		}
	}

	printf("Opened device bus=%d dev=%d\n",
	       libusb_get_bus_number(libusb_get_device(handle)),
	       libusb_get_device_address(libusb_get_device(handle)));

	r = libusb_get_device_descriptor(libusb_get_device(handle), &desc);
	if (r)
		exit(1);
	printf("bcdUSB=0x%04x idVendor=0x%04x idProduct=0x%04x bcdDevice=0x%04x\n",
	       desc.bcdUSB, desc.idVendor, desc.idProduct, desc.bcdDevice);

	r = libusb_get_configuration(handle, &config);
	if (r)
		usb_failed("libusb_get_configuration", r);
	printf("configuration=%d\n", config);
	if (config == 0) {
		printf("Selecting configuration 1 ...\n");
		r = libusb_set_configuration(handle, 1);
		if (r)
			usb_failed("libusb_set_configuration", r);
	} else if (config != 1) {
		fprintf(stderr, "Device in unknown configuration.\n");
		exit(1);
	}

	kdrv = libusb_kernel_driver_active(handle, 0);
	if (kdrv) {
		printf("Detaching kernel driver ...\n");
		r = libusb_detach_kernel_driver(handle, 0);
		if (r)
			usb_failed("libusb_detach_kernel_driver", r);
	}

	r = libusb_claim_interface(handle, 1);
	if (r)
		usb_failed("libusb_claim_interface", r);

	printf("Switch to text mode ... ");
	k = usb_flush(handle);
	usb_write(handle, "\0\0\0", 3);
	k += usb_flush(handle);
	printf("flushed %d bytes.\n", k);

	printf("Testing streaming RX %d bytes ... \n", BLASTRXSIZE);
	usleep(100000);
	for (i = 0; i < USB_NUMURB; i++)
		urb[i] = libusb_alloc_transfer(0);
	bcnt[0] = 0;
	bcnt[2] = 0;
	bcnt[3] = 0;
	timer_start(&tv);
	usb_write(handle, "\006", 1);
	for (i = 0; i < USB_NUMURB; i++) {
		libusb_fill_bulk_transfer(urb[i],
		                          handle, USB_ENDPT_RX,
		                          (unsigned char *)buf + (i * USB_URBSIZE), USB_URBSIZE,
		                          usb_streamrx_cb, bcnt,
		                          0);
		r = libusb_submit_transfer(urb[i]);
		if (r)
			usb_failed("libusb_submit_transfer", r);
	}
	while (bcnt[3] == 0)
		libusb_handle_events_completed(ctx, &bcnt[3]);
	elapsed = timer_elapsed(&tv);
	printf("  RX %8d bytes in %6.3f s = %8.0f bytes/s\n",
	       bcnt[0], elapsed, bcnt[0] / elapsed);
	for (i = 0; i < USB_NUMURB; i++)
		libusb_cancel_transfer(urb[i]);
	bcnt[3] = 0;
	while (bcnt[2] < USB_NUMURB)
		libusb_handle_events_completed(ctx, &bcnt[3]);
	for (i = 0; i < USB_NUMURB; i++)
		libusb_free_transfer(urb[i]);
	usb_flush(handle);

	printf("Estimating loss-rate at 25 MHz RX ... \n");
	usleep(100000);
	for (i = 0; i < USB_NUMURB; i++)
		urb[i] = libusb_alloc_transfer(0);
	bcnt[0] = 0;
	bcnt[2] = 0;
	bcnt[3] = 0;
	usb_write(handle, "\007", 1);
	for (i = 0; i < USB_NUMURB; i++) {
		libusb_fill_bulk_transfer(urb[i],
		                          handle, USB_ENDPT_RX,
		                          (unsigned char *)buf + (i * USB_URBSIZE), USB_URBSIZE,
		                          usb_lossrx_cb, bcnt,
		                          1000);
		r = libusb_submit_transfer(urb[i]);
		if (r)
			usb_failed("libusb_submit_transfer", r);
	}
	while (bcnt[3] == 0) {
		struct timeval tv = { 1, 0 }; 
		libusb_handle_events_timeout_completed(ctx, &tv, &bcnt[3]);
	}
	printf("  lost %d out of %d bytes = %3.3f %%\n",
               BLASTRXSIZE - bcnt[0], BLASTRXSIZE,
	       100.0 * (BLASTRXSIZE - bcnt[0]) / (double)BLASTRXSIZE);
	for (i = 0; i < USB_NUMURB; i++)
		libusb_free_transfer(urb[i]);
	usb_flush(handle);

	printf("Testing streaming TX %d bytes ... \n", BLASTTXSIZE);
	usleep(100000);
	for (i = 0; i < USB_NUMURB; i++)
		urb[i] = libusb_alloc_transfer(0);
	bcnt[0] = bcnt[1] = 0;
	timer_start(&tv);
	for (i = 0; i < USB_NUMURB && bcnt[1] < BLASTTXSIZE; i++) {
		k = BLASTTXSIZE - bcnt[1];
		if (k > USB_URBSIZE)
			k = USB_URBSIZE;
		bcnt[1] += k;
		memset(buf + (i * USB_URBSIZE), 0, k);
		libusb_fill_bulk_transfer(urb[i],
		                          handle, USB_ENDPT_TX,
		                          (unsigned char *)buf + (i * USB_URBSIZE), k,
		                          usb_streamtx_cb, bcnt,
		                          0);
		r = libusb_submit_transfer(urb[i]);
		if (r)
			usb_failed("libusb_submit_transfer", r);
	}
	while (bcnt[0] < BLASTTXSIZE)
		libusb_handle_events(ctx);
	elapsed = timer_elapsed(&tv);
	printf("  TX %8d bytes in %6.3f s = %8.0f bytes/s\n",
	       bcnt[0], elapsed, bcnt[0] / elapsed);
	for (i = 0; i < USB_NUMURB; i++)
		libusb_free_transfer(urb[i]);
	usb_flush(handle);

	r = libusb_release_interface(handle, 1);
	if (r)
		usb_failed("libusb_release_interface", r);

	if (kdrv) {
		printf("Re-attaching kernel driver ...\n");
		r = libusb_attach_kernel_driver(handle, 0);
		if (r)
			usb_failed("libusb_attach_kernel_driver", r);
	}

	if (config == 0) {
		printf("Unconfiguring device ...\n");
		r = libusb_set_configuration(handle, 0);
		if (r)
			usb_failed("libusb_set_configuration", r);
	}

	libusb_close(handle);
	libusb_exit(ctx);

	printf("Done.\n");
}


static void usage(void)
{
	fprintf(stderr,
	  "Usage:\n"
	  "  perftest -t /dev/ttyACMn\n"
	  "  perftest -d vendorid:productid	   (IDs in hex)\n"
	  "  perftest -s bus:devnum		   (Numbers in decimal)\n");
	exit(1);
}


int main(int argc, const char **argv)
{
	unsigned long vendorid, productid;
	unsigned long busnum, devnum;
	char *p;

	/* Make output stream unbuffered. */
	setbuf(stdout, NULL);

	if (argc == 3 && strcmp(argv[1], "-t") == 0) {
		test_serial(argv[2]);
	} else if (argc == 3 && strcmp(argv[1], "-d") == 0) {
		vendorid = strtoul(argv[2], &p, 16);
		if (p == argv[2] || *p != ':')
			usage();
		productid = strtoul(p + 1, &p, 16);
		if (*p != '\0')
			usage();
		test_libusb(vendorid, productid, -1, -1);
	} else if (argc == 3 && strcmp(argv[1], "-s") == 0) {
		busnum = strtoul(argv[2], &p, 10);
		if (p == argv[2] || *p != ':')
			usage();
		devnum = strtoul(p + 1, &p, 10);
		if (*p != '\0')
			usage();
		test_libusb(-1, -1, busnum, devnum);
	} else {
		usage();
	}

	return 0;
}

/* end */
