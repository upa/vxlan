#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <linux/if_ether.h>
#include <ifaddrs.h>

#include "net.h"
#include "fdb.h"
#include "error.h"


struct in6_addr *
is_ip6_ns (struct ether_header * ether)
{
	struct ip6_hdr * ip6_hdr;
	struct nd_neighbor_solicit * nd_ns;

	if (htons (ether->ether_type) != ETHERTYPE_IPV6)
		return NULL;
	
	ip6_hdr = (struct ip6_hdr *) (ether + 1);
	if (ip6_hdr->ip6_nxt != IPPROTO_ICMPV6)
		return NULL;
	
	nd_ns = (struct nd_neighbor_solicit *) (ip6_hdr + 1);
	if (nd_ns->nd_ns_type != ND_NEIGHBOR_SOLICIT)
		return NULL;

	return &nd_ns->nd_ns_target;
}

struct in_addr *
is_ip4_arp (struct ether_header * ether)
{
	struct ether_arp * arp;

	if (htons (ether->ether_type) != ETHERTYPE_ARP)
		return NULL;
	
	arp = (struct ether_arp *) (ether + 1);
	if (arp->arp_op != ARPOP_REQUEST)
		return NULL;

	return (struct in_addr *) (arp->arp_tpa);
}

void
process_fdb_etherflame_from_vxlan (struct vxlan_instance * vins,
				   struct ether_header * ether, 
				   struct sockaddr_storage * vtep_addr)
{
	struct fdb_entry * entry;
	
	entry = fdb_search_entry (vins->fdb, (u_int8_t *) ether->ether_shost);

	if (entry == NULL) {
		fdb_add_entry (vins->fdb, (u_int8_t *) ether->ether_shost, *vtep_addr);
	}
	else {
		if (COMPARE_SOCKADDR (vtep_addr, &entry->vtep_addr)) {
			entry->ttl = vins->fdb->fdb_max_ttl;
		} else {
			entry->vtep_addr = * vtep_addr;
			entry->ttl = vins->fdb->fdb_max_ttl;
		}
	}

	return;
}


void
send_etherflame_from_vxlan_to_local (struct vxlan_instance * vins, 
				     struct ether_header * ether, int len)
{
	
	if (write (vins->tap_sock, ether, len) < 0) {
		error_warn("Write etherflame to local network failed");
	}

	return;
}

void
send_etherflame_from_local_to_vxlan (struct vxlan_instance * vins, 
				     struct ether_header * ether, int len)
{
	struct in_addr * ip4;
	struct in6_addr * ip6;
	struct vxlan_hdr vhdr;
	struct fdb_entry * entry;
	struct msghdr mhdr;
	struct iovec iov[2];
	
	/* ACL Match */
	if (search_hash (&vins->acl_mac, ether->ether_shost) != NULL) 
		return;

	if ((ip4 = is_ip4_arp (ether)) != NULL) {
		if (search_hash (&vins->acl_ip4, ip4) != NULL)
			return;
	}

	if ((ip6 = is_ip6_ns (ether)) != NULL) {
		if (search_hash (&vins->acl_ip6, ip6) != NULL)
			return;
	}

	/* Forwarding */
	memset (&vhdr, 0, sizeof (vhdr));
	vhdr.vxlan_flags = VXLAN_VALIDFLAG;
	memcpy (vhdr.vxlan_vni, vins->vni, VXLAN_VNISIZE);

	iov[0].iov_base = &vhdr;
	iov[0].iov_len  = sizeof (vhdr);
	iov[1].iov_base = ether;
	iov[1].iov_len  = len;

	mhdr.msg_iov = iov;
	mhdr.msg_iovlen = 2;

	if ((entry = fdb_search_entry (vins->fdb, ether->ether_dhost)) == NULL) {
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
		err (EXIT_FAILURE, "can not get interface \"%s\" info", dev);

	close (fd);

	addr = (struct sockaddr_in *) &(ifr.ifr_addr);

	return addr->sin_addr;
}

struct in6_addr
getifaddr6 (char * dev)
{
        int fd;
        struct ifreq ifr;
        struct sockaddr_in6 * addr6;

        fd = socket (AF_INET, SOCK_DGRAM, 0);

        memset (&ifr, 0, sizeof (ifr));
        strncpy (ifr.ifr_name, dev, IFNAMSIZ - 1);
        ifr.ifr_addr.sa_family = AF_INET6;

        if (ioctl (fd, SIOCGIFADDR, &ifr) < 0)
                err (EXIT_FAILURE, "can not get interface \"%s\" info", dev);

        close (fd);

        addr6 = (struct sockaddr_in6 *) &(ifr.ifr_addr);

        return addr6->sin6_addr;
}

void
set_ipv4_multicast_join_and_iface (int socket, struct in_addr maddr, char * ifname)
{
	char cmaddr[16];
	struct ip_mreq mreq;
	
	memset (&mreq, 0, sizeof (mreq));
	mreq.imr_multiaddr = maddr;
	mreq.imr_interface = getifaddr (ifname);
	
	if (setsockopt (socket,
			IPPROTO_IP,
			IP_ADD_MEMBERSHIP,
			(char *)&mreq, sizeof (mreq)) < 0) {
		inet_ntop (AF_INET, &maddr, cmaddr, sizeof (cmaddr));
		err (EXIT_FAILURE, "can not join multicast %s", cmaddr);
	}

	if (setsockopt (socket,
			IPPROTO_IP,
			IP_MULTICAST_IF,
			(char *)&mreq.imr_interface,
			sizeof (mreq.imr_interface)) < 0)
		err (EXIT_FAILURE, "can not set multicast interface");

	return;
}


void 
set_ipv6_multicast_join_and_iface (int socket, struct in6_addr maddr, char * ifname)
{
	struct ipv6_mreq mreq6;
	char cmaddr[48];
	
	memset (&mreq6, 0, sizeof (mreq6));
	mreq6.ipv6mr_multiaddr = maddr;
	mreq6.ipv6mr_interface = if_nametoindex (ifname);
	
	if (mreq6.ipv6mr_interface == 0) 
		err (EXIT_FAILURE, "invalid interface \"%s\"", ifname);

	if (setsockopt (socket,
			IPPROTO_IPV6,
			IPV6_ADD_MEMBERSHIP,
			(char *)&mreq6, sizeof (mreq6)) < 0) {
		inet_ntop (AF_INET6, &maddr, cmaddr, sizeof (cmaddr));
		err (EXIT_FAILURE, "can not join multicast %s", cmaddr);
	}

	if (setsockopt (socket,
			IPPROTO_IPV6,
			IPV6_MULTICAST_IF,
			(char *)&mreq6.ipv6mr_interface,
			sizeof (mreq6.ipv6mr_interface)) < 0) {
		err (EXIT_FAILURE,
		     "can not set multicast interface \"%s\"", 
		     if_indextoname (mreq6.ipv6mr_interface, ifname));
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

void
bind_ipv4_inaddrany (int socket, int port)
{
	struct sockaddr_in saddr_in;
	
	saddr_in.sin_family = AF_INET;
	saddr_in.sin_port = htons (port);
	saddr_in.sin_addr.s_addr = INADDR_ANY;
	
	if (bind (socket, (struct sockaddr *)&saddr_in, sizeof (saddr_in)) < 0)
		err (EXIT_FAILURE, "can not bind");
	
	return;
}

void
bind_ipv6_inaddrany (int socket, int port)
{
	struct sockaddr_in6 saddr_in6;
	
	saddr_in6.sin6_family = AF_INET6;
	saddr_in6.sin6_port = htons (port);
	saddr_in6.sin6_addr = in6addr_any;

	if (bind (socket, (struct sockaddr *)&saddr_in6, sizeof (saddr_in6)) < 0)
		err (EXIT_FAILURE, "can not bind");

	return;
}

void
bind_ipv6_addr (int socket, struct in6_addr addr6, int port)
{
	struct sockaddr_in6 saddr6;
	
	memset (&saddr6, 0, sizeof (saddr6));
	saddr6.sin6_family = AF_INET6;
	saddr6.sin6_port = htons (port);
	saddr6.sin6_addr = addr6;
	
	if (bind (socket, (struct sockaddr *)&saddr6, sizeof (saddr6)) != 0) {
		err (EXIT_FAILURE, "can not bind IPv6 addr");
	}
	return;
}

void
set_ipv6_pktinfo (int socket, int stat)
{
	if (setsockopt (socket,
			IPPROTO_IPV6, 
			IPV6_PKTINFO, 
			&stat, 
			sizeof (stat)) < 0)
		err (EXIT_FAILURE, "can not set IPV6_PKTINFO");

	return;
}


int
ifaddr (int ai_family , char * dev, void * dest)
{
	struct ifaddrs * ifa;
	struct ifaddrs * ifa_list;
	struct sockaddr_in * saddr_in;
	struct sockaddr_in6 * saddr_in6;
	
	if (getifaddrs (&ifa_list) != 0) {
		err (EXIT_FAILURE, "getifaddrs");
	}

	for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next) {
		if (strncmp (dev, ifa->ifa_name, IFNAMSIZ) != 0) 
			continue;

		if (ai_family != ifa->ifa_addr->sa_family)
			continue;

		switch (ifa->ifa_addr->sa_family) {
		case AF_INET :
			if (ifa->ifa_addr->sa_family != AF_INET) 
				continue;

			saddr_in = (struct sockaddr_in *)(ifa->ifa_addr);
			memcpy (dest, 
				&saddr_in->sin_addr,
				sizeof (struct in_addr));
			freeifaddrs (ifa_list);

			return 1;

		case AF_INET6 :
			if (ifa->ifa_addr->sa_family != AF_INET6) 
				continue;

			if (IN6_IS_ADDR_LINKLOCAL (&(((struct sockaddr_in6 *)
						      ifa->ifa_addr)->sin6_addr)))
				continue;
			saddr_in6 = (struct sockaddr_in6 *)(ifa->ifa_addr);
			memcpy (dest, 
				&saddr_in6->sin6_addr,
				sizeof (struct in6_addr));
			freeifaddrs (ifa_list);
			return 1;
		}
	}


	freeifaddrs (ifa_list);		
	return -1;
}
