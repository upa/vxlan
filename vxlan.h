#ifndef _VXLAN_H_
#define _VXLAN_H_

#include "common.h"

void strtovni (char * str, u_int8_t * vni);

struct vxlan_instance * create_vxlan_instance (u_int8_t * vni, char * configfile);
void init_vxlan_instance (struct vxlan_instance * vins);

void process_fdb_etherflame_from_vxlan (struct vxlan_instance * vins,
                                        struct ether_header * ether,
                                        struct sockaddr_storage * vtep_addr);

void send_etherflame_from_vxlan_to_local (struct vxlan_instance * vins,
                                          struct ether_header * ether, int len);

void send_etherflame_from_local_to_vxlan (struct vxlan_instance * vins,
                                          struct ether_header * ether, int len);


#endif
