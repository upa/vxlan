#ifndef _COMMON_H_ 
#define _COMMON_H_

#include <sys/types.h>
#include <netinet/in.h>
#include "hash.h"

#define VXLAN_PORT 6000
#define VXLAN_MCAST_PORT 6001

struct vxlan {
	int tap_sock;
	int udp_sock;
	int mst_send_sock;
	int mst_recv_sock;
	
	u_int8_t vni[3];
	struct in_addr  mcast_addr;	/* vxlan Multicast Address */
	struct sockaddr mcast_saddr;	/* vxlan Multicast Address in Sockaddr  */

	struct hash fdb;
};

extern struct vxlan vxlan;



struct vxlan_hdr {
	u_int8_t vxlan_flags;
	u_int8_t vxlan_rsv1[3];
	u_int8_t vxlan_vni[3];
	u_int8_t vxlan_rsv2;
};

#define VXLAN_VALIDFLAG 0x08
#define VXLAN_VNISIZE	3


#define VXLAN_TUNNAME "vxlan"

#define VXLAN_PACKET_BUF_LEN 1600

#endif
