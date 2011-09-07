#ifndef _NET_H_
#define _NET_H_

#include <netinet/in.h>
#include <net/ethernet.h>
#include <netinet/if_ether.h>
#include <netinet/icmp6.h>

int udp_sock (int port);
int mcast_sock (int port, struct in_addr mcast_if_addr, struct in_addr mcast_addr);
struct in_addr getifaddr (char * dev);


void process_fdb_etherflame_from_vxlan (struct ether_header * ether,
				       struct in_addr * vtep_addr);
void send_etherflame_from_vxlan_to_local (struct ether_header * ether, int len);
void send_etherflame_from_local_to_vxlan (struct ether_header * ether, int len);



#endif /* _NET_H_ */
