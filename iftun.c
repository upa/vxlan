/* iftun.c */

#include "iftun.h"
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
tun_alloc (char * dev)
{
#ifdef linux
	/* Create Tunnel Interface Linux */

	int fd;
	struct ifreq ifr;

	if ((fd = open ("/dev/net/tun", O_RDWR)) < 0)
		err (EXIT_FAILURE, 
		     "cannot create a control cahnnel of the tun intface.");

	memset (&ifr, 0, sizeof (ifr));
	ifr.ifr_flags = IFF_TUN;
	strncpy (ifr.ifr_name, dev, IFNAMSIZ);

	if (ioctl (fd, TUNSETIFF, (void *) &ifr) < 0) {
		close (fd);
		err (EXIT_FAILURE, 
		     "cannot create %s interface.", dev);
	}

	return fd;

#else
	/* Create Tunnel Interface BSD */

	int fd, ctl_fd;
	char tun_dev_path[IFNAMSIZ + strlen ("/dev/") + 1];
	struct ifreq ifr;

	if ((ctl_fd = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
		err (EXIT_FAILURE, "cannot create a control socket.");

	memset (&ifr, 0, sizeof (ifr));
	strncpy (ifr.ifr_name, dev, IFNAMSIZ);
	if (ioctl (ctl_fd, SIOCIFCREATE2, &ifr, IFNAMSIZ) < 0)
		err (EXIT_FAILURE, "cannot create interface %s.", dev);

	sprintf (tun_dev_path, "/dev/%s", dev);

	if ((fd = open (tun_dev_path, O_RDWR)) < 0)
		err (EXIT_FAILURE, "cannnot open a interface %s.", tun_dev_path);
	
	close (ctl_fd);

	return fd;
#endif
}

int
tun_up (char * dev)
{
	int udp_fd;
	struct ifreq ifr;

	/* Make Tunnel interface up state */

	if ((udp_fd = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
		err (EXIT_FAILURE,
		     "failt to create control socket of tun interface.");

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

u_int32_t
tun_get_af (const void * buf)
{
	struct tun_pi * pi = (struct tun_pi *) buf;
	u_int16_t ether_type = ntohs (pi->proto);
	
	switch (ether_type) {
	case ETH_P_IP :
		return AF_INET;

	case ETH_P_IPV6 :
		return AF_INET6;

	default :
		return 0;
	}

	/* Not reach */
	return 0;
}

int
tun_set_af(void *buf, uint32_t af)
{
	assert(buf != NULL);

	uint16_t ether_type;

	switch(af) {
	case AF_INET:
		ether_type = ETH_P_IP;
		break;
	case AF_INET6:
		ether_type = ETH_P_IPV6;
		break;
	default:
		warnx("unsupported address family %d", af);
		return (-1);
	}

	struct tun_pi *pi = buf;
	pi->flags = 0;
	pi->proto = htons(ether_type);

	return (0);

	uint32_t *af_space = buf;

	*af_space = htonl(af);

	return (0);
}


#ifndef linux

int
tun_get_next_intnum (void)
{
	int  ifnum, tunnum = -1;
	char ifstr[IFNAMSIZ];
	struct if_nameindex * ifindex, * if1index;

	if ((ifindex = if_nameindex ()) == NULL)
		err (EXIT_FAILURE, "cannot get interface list");

	if1index = ifindex;

	for (;ifindex != NULL && ifindex->if_index != 0; ifindex++) {
		if (sscanf (ifindex->if_name, "%s%d", ifstr, &ifnum) < 2)
			continue;
		if (strncmp ("tun", ifstr, 3) == 0)
			tunnum = (tunnum < ifnum) ? ifnum : tunnum;
	}

	if_freenameindex (if1index);

	return tunnum + 1;
}

#endif
