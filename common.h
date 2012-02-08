#ifndef _COMMON_H_ 
#define _COMMON_H_

#include <sys/types.h>
#include <netinet/in.h>
#include "hash.h"

#define VXLAN_PORT_BASE		6000
#define VXLAN_VALIDFLAG 	0x08
#define VXLAN_VNISIZE		3
#define VXLAN_TUNNAME		"vxlan"
#define VXLAN_PACKET_BUF_LEN	9216


struct vxlan {
	int tap_sock;
	int udp_sock;

	unsigned short port;
	u_int8_t vni[3];
	struct sockaddr_storage mcast_addr; 	/* vxlan Multicast Address */

	struct fdb * fdb;
};

extern struct vxlan vxlan;


struct vxlan_hdr {
	u_int8_t vxlan_flags;
	u_int8_t vxlan_rsv1[3];
	u_int8_t vxlan_vni[3];
	u_int8_t vxlan_rsv2;
};


#endif
