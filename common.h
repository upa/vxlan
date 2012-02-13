#ifndef _COMMON_H_ 
#define _COMMON_H_

#include <sys/types.h>
#include <netinet/in.h>
#include "hash.h"

#define VXLAN_PORT_BASE		60000
#define VXLAN_VALIDFLAG 	0x08
#define VXLAN_VNISIZE		3
#define VXLAN_MCAST_TTL		16
#define VXLAN_TUNNAME		"vxlan"
#define VXLAN_PACKET_BUF_LEN	9216


struct vxlan_instance {
	u_int8_t vni[VXLAN_VNISIZE];
	char vxlan_tap_name[IFNAMSIZ];

	struct fdb * fdb;

	pthread_t tid;
	int tap_sock;
};


struct vxlan {
	int udp_sock;

	int subnum;
	unsigned short port;
	struct sockaddr_storage mcast_addr; 	/* vxlan Multicast Address */

	int vins_num;
	struct hash vins_tuple;
	struct vxlan_instance ** vins;
};


extern struct vxlan vxlan;



struct vxlan_hdr {
	u_int8_t vxlan_flags;
	u_int8_t vxlan_rsv1[3];
	u_int8_t vxlan_vni[3];
	u_int8_t vxlan_rsv2;
};


#endif
