#include "net.h"
#include "common.h"
#include "fdb.h"

#include <err.h>
#include <stdlib.h>
#include <unistd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>

int
udp_sock (int port)
{
	int sock;
	struct sockaddr_in saddr_in;
	
	if ((sock = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
		err (EXIT_FAILURE, "can not create udp socket");
	
	saddr_in.sin_family = AF_INET;
	saddr_in.sin_port = htons (port);
	saddr_in.sin_addr.s_addr = INADDR_ANY;

	if (bind (sock, (struct sockaddr *)&saddr_in, sizeof (saddr_in)) < 0)
		err (EXIT_FAILURE, "can not bind udp socket");


	return sock;
}

int
mcast_sock (int port, struct in_addr mcast_if_addr, struct in_addr mcast_addr) 
{
	int sock;
	struct sockaddr_in saddr_in;
	struct ip_mreq mreq;
	
	if ((sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		err (EXIT_FAILURE, "can not create multicast socket");
	
	saddr_in.sin_family = AF_INET;
	saddr_in.sin_port = htons (port);
	saddr_in.sin_addr.s_addr = INADDR_ANY;
	
	memset (&mreq, 0, sizeof (mreq));
	mreq.imr_interface = mcast_if_addr;
	mreq.imr_multiaddr = mcast_addr;
	
	if (setsockopt (sock,
			IPPROTO_IP,
			IP_ADD_MEMBERSHIP, 
			(char *)&mreq, sizeof (mreq)) < 0)
		err (EXIT_FAILURE, "mcast recieve socket creation failed");
	
	if (setsockopt (sock,
			IPPROTO_IP,
			IP_MULTICAST_IF,
			(char *)&mcast_if_addr, sizeof (mcast_if_addr)) < 0)
		err (EXIT_FAILURE, "mcast send socket creation failed");

	return sock;
}

struct in_addr
getifaddr (char * dev)
{
	int fd;
	struct ifreq ifr;
	struct sockaddr_in * addr;

	fd = socket (AF_INET, SOCK_DGRAM, 0);

	memset (&ifr, 0, sizeof (ifr));
	strncpy (ifr.ifr_name, dev, IFNAMSIZ - 1);

	if (ioctl (fd, SIOCGIFADDR, &ifr) < 0)
		err (EXIT_FAILURE, "getifaddr");

	close (fd);

	addr = (struct sockaddr_in *) &(ifr.ifr_addr);

	return addr->sin_addr;
}

void
process_fdb_etherflame_from_vxlan (struct ether_header * ether, struct in_addr * vtep_addr)
{
	struct fdb_entry * entry;
	struct sockaddr_in * saddr_in;
	
	entry = fdb_search_entry (&vxlan.fdb, (u_int8_t *) ether->ether_shost);
	if (entry == NULL)
		fdb_add_entry (&vxlan.fdb, (u_int8_t *) ether->ether_shost, *vtep_addr);
	else {
		saddr_in = (struct sockaddr_in *) &entry->vtep_addr;
		if (saddr_in->sin_addr.s_addr != vtep_addr->s_addr) {
			saddr_in->sin_addr = *vtep_addr;
		}
		entry->ttl = FDB_CACHE_TTL;
	}

	return;
}

void
send_etherflame_from_vxlan_to_local (struct ether_header * ether, int len)
{
	if (write (vxlan.tap_sock, ether, len) < 0) {
		warn ("write etherflame to local network failed");
	}

	return;
}

#if 0
void
send_etherflame_from_local_to_vxlan (struct ether_header * ether, int len)
{
	struct vxlan_hdr vhdr;
	struct msghdr mhdr;
	struct iovec iov[2];
	struct fdb_entry * entry;
	
	memset (&vhdr, 0, sizeof (vhdr));
	vhdr.vxlan_flags = VXLAN_VALIDFLAG;
	memcpy (vhdr.vxlan_vni, vxlan.vni, VXLAN_VNISIZE);

	iov[0].iov_base = &vhdr;
	iov[0].iov_len  = sizeof (vhdr);
	iov[1].iov_base = ether;
	iov[1].iov_len  = len;

	if ((entry = fdb_search_entry (&vxlan.fdb, (u_int8_t *)ether->ether_dhost)) == NULL) {
		mhdr.msg_name = &vxlan.mcast_saddr;
		mhdr.msg_namelen = sizeof (vxlan.mcast_saddr);
		mhdr.msg_iov = iov;
		mhdr.msg_iovlen = 2;
		mhdr.msg_controllen = 0;

		if (sendmsg (vxlan.mst_sock, &mhdr, 0) < 0) 
			warn ("sendmsg to Multicast failed");
		
	} else {
		mhdr.msg_name = &entry->vtep_addr;
		mhdr.msg_namelen = sizeof (entry->vtep_addr);
		mhdr.msg_iov = iov;
		mhdr.msg_iovlen = 2;
		mhdr.msg_controllen = 0;

		if (sendmsg (vxlan.udp_sock, &mhdr, 0) < 0) 
			warn ("sendmsg to unicast failed");
		
	}
	
	return;
}
#endif

void
send_etherflame_from_local_to_vxlan (struct ether_header * ether, int len)
{
	int n;
	struct vxlan_hdr vhdr;
	struct fdb_entry * entry;
	
	char buf[2048];

	memset (&vhdr, 0, sizeof (vhdr));
	vhdr.vxlan_flags = VXLAN_VALIDFLAG;
	memcpy (vhdr.vxlan_vni, vxlan.vni, VXLAN_VNISIZE);

	memcpy (buf, &vhdr, sizeof (struct vxlan_hdr));
	memcpy (buf + sizeof (struct vxlan_hdr), ether, len);

	if ((entry = fdb_search_entry (&vxlan.fdb, (u_int8_t *)ether->ether_dhost)) == NULL) {
		n = sendto (vxlan.mst_sock, buf, sizeof (struct vxlan_hdr) + len, 0,
			    (struct sockaddr *)&vxlan.mcast_saddr, sizeof (vxlan.mcast_saddr));
		if (n < 0) warn ("sendto multicsat faield");

	} else {
		n = sendto (vxlan.udp_sock, buf, sizeof (struct vxlan_hdr) + len, 0,
			    &entry->vtep_addr, sizeof (entry->vtep_addr));
		if (n < 0) warn ("sendto unicast faield");
	}
	
	return;
}


