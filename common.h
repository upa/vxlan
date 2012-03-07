#ifndef _COMMON_H_ 
#define _COMMON_H_

#include <net/if.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "hash.h"

#define VXLAN_PORT_BASE		60000
#define VXLAN_CPORT		"60000"
#define VXLAN_VALIDFLAG 	0x08
#define VXLAN_VNISIZE		3
#define VXLAN_MCAST_TTL		16
#define VXLAN_TUNNAME		"vxlan"
#define VXLAN_PACKET_BUF_LEN	9216


#define VXLAN_LOGNAME		"vxlan"
#define VXLAN_LOGFACILITY	LOG_LOCAL7

#define ACL_MASK_RA	0x01
#define ACL_MASK_RS	0x02

struct vxlan_instance {
	u_int8_t vni[VXLAN_VNISIZE];
	char vxlan_tap_name[IFNAMSIZ];

	struct fdb * fdb;
	struct hash acl_mac;
	struct hash acl_ip4;
	struct hash acl_ip6;
	u_int8_t acl_mask;

	pthread_t tid;
	int tap_sock;
};


struct vxlan {
	int udp_sock;
	int unicast_sock;

	unsigned short port;
	struct sockaddr_storage mcast_addr; 	/* vxlan Multicast Address */

	int vins_num;			/* Num of VXLAN Instance */
	struct hash vins_tuple;		/* VXLAN Instance hash table. key is VNI */
	struct vxlan_instance ** vins;	/* VXLAN Instance List */
};


extern struct vxlan vxlan;



struct vxlan_hdr {
	u_int8_t vxlan_flags;
	u_int8_t vxlan_rsv1[3];
	u_int8_t vxlan_vni[3];
	u_int8_t vxlan_rsv2;
};


#endif
