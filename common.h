#ifndef _COMMON_H_ 
#define _COMMON_H_

#include <sys/types.h>
#include <netinet/in.h>
#include "hash.h"

#define VXLAN_PORT 6000

struct vxlan {
	int port;
	int tap_sock;
	int udp_sock;
	
	u_int8_t vni[3];
	struct in_addr maddr;

	struct hash fdb;
};

extern struct vxlan vxlan;



struct vxlan_hdr {
	u_int8_t vxlan_flags;
	u_int8_t vxlan_rsv1[3];
	u_int8_t vxlan_vni[3];
	u_int8_t vxlan_rsv2;
};

#define VXLAN_VALIDFLAG 0x08;





#endif
