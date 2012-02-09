#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>

#include "net.h"
#include "common.h"
#include "fdb.h"
#include "error.h"


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
		err (EXIT_FAILURE, "can not get interface %s info", dev);

	close (fd);

	addr = (struct sockaddr_in *) &(ifr.ifr_addr);

	return addr->sin_addr;
}

void
process_fdb_etherflame_from_vxlan (struct ether_header * ether, 
				   struct sockaddr_storage * vtep_addr)
{
	struct fdb_entry * entry;
	
	entry = fdb_search_entry (vxlan.fdb, (u_int8_t *) ether->ether_shost);

	if (entry == NULL)
		fdb_add_entry (vxlan.fdb, (u_int8_t *) ether->ether_shost, *vtep_addr);
	else {
		if (COMPARE_SOCKADDR (vtep_addr, &entry->vtep_addr)) {
			entry->ttl = vxlan.fdb->fdb_max_ttl;
		} else {
			entry->vtep_addr = * vtep_addr;
			entry->ttl = vxlan.fdb->fdb_max_ttl;
		}
	}

	return;
}

void
send_etherflame_from_vxlan_to_local (struct ether_header * ether, int len)
{
	if (write (vxlan.tap_sock, ether, len) < 0) {
		error_warn("Write etherflame to local network failed");
	}

	return;
}

void
send_etherflame_from_local_to_vxlan (struct ether_header * ether, int len)
{
	struct vxlan_hdr vhdr;
	struct fdb_entry * entry;
	struct msghdr mhdr;
	struct iovec iov[2];
	
	memset (&vhdr, 0, sizeof (vhdr));
	vhdr.vxlan_flags = VXLAN_VALIDFLAG;
	memcpy (vhdr.vxlan_vni, vxlan.vni, VXLAN_VNISIZE);

	iov[0].iov_base = &vhdr;
	iov[0].iov_len  = sizeof (vhdr);
	iov[1].iov_base = ether;
	iov[1].iov_len  = len;

	mhdr.msg_iov = iov;
	mhdr.msg_iovlen = 2;
	mhdr.msg_controllen = 0;

	if ((entry = fdb_search_entry (vxlan.fdb, (u_int8_t *)ether->ether_dhost)) == NULL) {
		mhdr.msg_name = &vxlan.mcast_addr;
		mhdr.msg_namelen = sizeof (vxlan.mcast_addr);
	} else {
		mhdr.msg_name = &entry->vtep_addr;
		mhdr.msg_namelen = sizeof (entry->vtep_addr);
	}
	
	if (sendmsg (vxlan.udp_sock, &mhdr, 0) < 0) 
		error_warn("sendmsg to multicast failed");

	return;
}



void 
set_ipv4_multicast_join (int socket, struct in_addr maddr)
{
	struct ip_mreq mreq;
	char cmaddr[16];
	
	memset (&mreq, 0, sizeof (mreq));
	mreq.imr_multiaddr = maddr;

	if (setsockopt (socket,
			IPPROTO_IP,
			IP_ADD_MEMBERSHIP,
			(char *)&mreq, sizeof (mreq)) < 0) {
		inet_ntop (AF_INET, &maddr, cmaddr, sizeof (cmaddr));
		err (EXIT_FAILURE, "can not join multicast %s", cmaddr);
	}

	return;
}

void 
set_ipv6_multicast_join (int socket, struct in6_addr maddr)
{
	struct ipv6_mreq mreq6;
	char cmaddr[48];
	
	memset (&mreq6, 0, sizeof (mreq6));
	mreq6.ipv6mr_multiaddr = maddr;
	
	if (setsockopt (socket,
			IPPROTO_IPV6,
			IPV6_ADD_MEMBERSHIP,
			(char *)&mreq6, sizeof (mreq6)) < 0) {
		inet_ntop (AF_INET6, &maddr, cmaddr, sizeof (cmaddr));
		err (EXIT_FAILURE, "can not join multicast %s", cmaddr);
	}

	return;
}

void
set_ipv4_multicast_iface (int socket, struct in_addr ifaddr)
{
	struct ip_mreq mreq;
	
	memset (&mreq, 0, sizeof (mreq));
	mreq.imr_interface = ifaddr;
	
	if (setsockopt (socket,
			IPPROTO_IP,
			IP_MULTICAST_IF,
			(char *)&mreq.imr_interface,
			sizeof (mreq.imr_interface)) < 0)
		err (EXIT_FAILURE, "can not set multicast interface");

	return;
}

void
set_ipv6_multicast_iface (int socket, unsigned int ifindex)
{
	struct ipv6_mreq mreq6;
	char ifname[IFNAMSIZ];
	
	memset (&mreq6, 0, sizeof (mreq6));
	mreq6.ipv6mr_interface = ifindex;

	if (setsockopt (socket,
			IPPROTO_IPV6,
			IPV6_MULTICAST_IF,
			(char *)&mreq6.ipv6mr_interface,
			sizeof (mreq6.ipv6mr_interface)) < 0) {
		err (EXIT_FAILURE,
		     "can not join multicast %s", 
		     if_indextoname (ifindex, ifname));
	}

	return;
}

void
set_ipv4_multicast_loop (int socket, int stat)
{
	if (setsockopt (socket,
			IPPROTO_IP,
			IP_MULTICAST_LOOP,
			(char *)&stat, sizeof (stat)) < 0)
		err (EXIT_FAILURE, "can not set off multicast loop");

	return;
}

void
set_ipv6_multicast_loop (int socket, int stat)
{
	if (setsockopt (socket,
			IPPROTO_IPV6,
			IPV6_MULTICAST_LOOP,
			(char *)&stat, sizeof (stat)) < 0)
		err (EXIT_FAILURE, "can not set off multicast loop");

	return;
}

void
set_ipv4_multicast_ttl (int socket, int ttl)
{
	if (setsockopt (socket,
			IPPROTO_IP,
			IP_MULTICAST_TTL,
			(char *)&ttl, sizeof (ttl)) < 0)
		err (EXIT_FAILURE, "can not set mutlicsat ttl");

	return;
}

void

set_ipv6_multicast_ttl (int socket, int ttl)
{
	if (setsockopt (socket,
			IPPROTO_IPV6,
			IPV6_MULTICAST_HOPS,
			(char *)&ttl, sizeof (ttl)) < 0)
		err (EXIT_FAILURE, "can not set ttl");

	return;
}

