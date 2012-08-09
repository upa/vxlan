#ifndef _COMMON_H_ 
#define _COMMON_H_

#include <net/if.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <uthash.h>

#define VXLAN_PORT_BASE		60000
#define VXLAN_CPORT		"60000"
#define VXLAN_VALIDFLAG 	0x08
#define VXLAN_VNISIZE		3
#define VXLAN_MCAST_TTL		16
#define VXLAN_TUNNAME		"vxlan"
#define VXLAN_PACKET_BUF_LEN	9216

#define VXLAN_LOGNAME		"vxlan"
#define VXLAN_LOGFACILITY	LOG_LOCAL7

#define VXLAN_UNIX_DOMAIN	"/var/run/vxlan"


struct vxlan_instance {
	/* uthash key */
	struct vnikey {		
		u_int8_t vni[VXLAN_VNISIZE];
	} vni;

	char vxlan_tap_name[IFNAMSIZ];
	struct fdb * fdb;
	pthread_t tid;			/* thread ID of process own VID instance */
	int tap_sock;

	UT_hash_handle hh;
};


struct vxlan {
	int udp_sock;
	int control_sock;

	unsigned short port;
	struct sockaddr_storage mcast_addr; 	/* vxlan Multicast Address */

	int vins_num;				/* Num of VXLAN Instance */
	struct vxlan_instance * vins_table; 	/* VXLAN Instance uthash table */
	pthread_t control_tid;			/* Control Thread ID */
};


extern struct vxlan vxlan;



struct vxlan_hdr {
	u_int8_t vxlan_flags;
	u_int8_t vxlan_rsv1[3];
	u_int8_t vxlan_vni[3];
	u_int8_t vxlan_rsv2;
};


#endif
