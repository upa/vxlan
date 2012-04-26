#ifndef _NET_H_
#define _NET_H_

#include <netinet/in.h>
#include <net/ethernet.h>
#include <netinet/if_ether.h>

#include "common.h"

void send_etherflame_from_vxlan_to_local (struct vxlan_instance * vins,
                                          struct ether_header * ether, int len);

void send_etherflame_from_local_to_vxlan (struct vxlan_instance * vins,
                                          struct ether_header * ether, int len);

struct in_addr getifaddr (char * dev);
struct in6_addr getifaddr6 (char * dev);
int ifaddr (int ai_family, char * dev, void * dest);

void set_ipv4_multicast_join_and_iface (int socket, struct in_addr maddr, char * ifname);
void set_ipv6_multicast_join_and_iface (int socket, struct in6_addr maddr, char * ifname);
void set_ipv4_multicast_loop (int socket, int stat);
void set_ipv6_multicast_loop (int socket, int stat);
void set_ipv6_multicast_ttl (int socket, int ttl);
void set_ipv4_multicast_ttl (int socket, int ttl);
void bind_ipv4_inaddrany (int socket, int port);
void bind_ipv6_inaddrany (int socket, int port);
void bind_ipv6_addr (int socket, struct in6_addr addr6, int port);

void set_ipv6_pktinfo (int socket, int stat);

#define CHECK_VNI(v1, v2)		\
	(v1[0] != v2[0]) ? -1 :		\
	(v1[1] != v2[1]) ? -1 :		\
	(v1[2] != v2[2]) ? -1 : 1	\


	




#endif /* _NET_H_ */
