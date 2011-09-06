#ifndef _NET_H_
#define _NET_H_

#include <netinet/in.h>
#include <net/ethernet.h>
#include <netinet/if_ether.h>
#include <netinet/icmp6.h>

int udp_sock (int port);
void process_ether (struct ether_header * ether, struct in_addr vtep_addr);





#endif /* _NET_H_ */
