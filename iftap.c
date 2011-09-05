/* iftap.c */

#include "iftan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <err.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <arpa/inet.h>

int
tap_alloc (char * dev)
{
	/* Create Tunnel Interface Linux */

	int fd;
	struct ifreq ifr;

	if ((fd = open ("/dev/net/tun", O_RDWR)) < 0)
		err (EXIT_FAILURE, 
		     "cannot create a control cahnnel of the tun intface.");

	memset (&ifr, 0, sizeof (ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	strncpy (ifr.ifr_name, dev, IFNAMSIZ);

	if (ioctl (fd, TUNSETIFF, (void *) &ifr) < 0) {
		close (fd);
		err (EXIT_FAILURE, 
		     "cannot create %s interface.", dev);
	}

	return fd;
}

int
tap_up (char * dev)
{
	int udp_fd;
	struct ifreq ifr;

	/* Make Tunnel interface up state */

	if ((udp_fd = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
		err (EXIT_FAILURE,
		     "failt to create control socket of tap interface.");

	memset (&ifr, 0, sizeof (ifr));
	ifr.ifr_flags = IFF_UP;
	strncpy (ifr.ifr_name, dev, IFNAMSIZ);

	if (ioctl (udp_fd, SIOCSIFFLAGS, (void *)&ifr) < 0) {
		err (EXIT_FAILURE,
		     "failed to make %s up.", dev);
		close (udp_fd);
		return -1;
	}

	close (udp_fd);

	return 0;
}
