#ifndef _VXLAN_H_
#define _VXLAN_H_

#include <net/ethernet.h>

#include "common.h"


void strtovni (char * str, u_int8_t * vni);


struct vxlan_instance * create_vxlan_instance (u_int8_t * vni);
struct vxlan_instance * search_vxlan_instance (u_int8_t * vni);
void init_vxlan_instance (struct vxlan_instance * vins);
int  add_vxlan_instance (struct vxlan_instance * vins);
int  destroy_vxlan_instance (struct vxlan_instance * vins);

void process_fdb_etherflame_from_vxlan (struct vxlan_instance * vins,
                                        struct ether_header * ether,
                                        struct sockaddr_storage * vtep_addr);

#endif
