#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <net/if.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>

#include "common.h"
#include "net.h"
#include "fdb.h"
#include "iftap.h"
#include "error.h"

struct vxlan vxlan;
unsigned short port = 0;

void process_vxlan (void);

void debug_print_vhdr (struct vxlan_hdr * vhdr);
void debug_print_ether (struct ether_header * ether);

void
usage (void)
{
	printf ("Usage\n");
	printf ("\t -v : VXLAN Network Identifier (24bit Hex)\n");
	printf ("\t -m : Multicast Address(v4/v6)\n");
	printf ("\t -i : Multicast Interface\n");
	printf ("\t -n : Sub-interface number (<4096)\n");
	printf ("\t -d : Daemon Mode\n");
	printf ("\n");
}


int
main (int argc, char * argv[])
{
	int ch;
	int d_flag = 0;
        int sockopt;
        int subn = 0;
	u_int32_t vni32;
	struct sockaddr_in * saddr_in;

	extern int opterr;	
	extern char * optarg;

	char mcast_caddr[40];
	char vxlan_if_name[IFNAMSIZ];
        char tunifname[IFNAMSIZ];
	
	memset (&vxlan, 0, sizeof (vxlan));

	while ((ch = getopt (argc, argv, "v:m:i:n:d")) != -1) {
		switch (ch) {
		case 'v' :
			if (optarg == NULL) {
				usage ();
				return -1;
			}

			vni32 = strtol (optarg, NULL, 16);
			if (vni32 == LONG_MAX) 
				err (EXIT_FAILURE, "strtol overflow");

                        if ( vni32 == LONG_MAX ) {
				err (EXIT_FAILURE, "strtol overflow");
                        } else {
				if ( vni32 == LONG_MIN ) {
					err (EXIT_FAILURE, "strtol underflow");
				} else {
					u_int8_t * vp = (u_int8_t *) &vni32;
					vxlan.vni[0] = *(vp + 3);
					vxlan.vni[1] = *(vp + 2);
					vxlan.vni[2] = *(vp + 1);
                            }
                        }
			break;

		case 'm' :
			if (optarg == NULL) {
				usage ();
				return -1;
			}
			strncpy (mcast_caddr, optarg, sizeof (mcast_caddr));

			break;

		case 'i' :
			if (optarg == NULL) {
				usage ();
				return -1;
			}
			strcpy (vxlan_if_name, optarg);
			
			break;

		case 'n' :
			if ( optarg == NULL ) {
				usage ();
				return -1;
			}
			subn = atoi(optarg);
			
			break;

		case 'd' :
			d_flag = 1;
			break;

		default :
			usage ();
			return -1;
		}
	}


        /* Enable syslog */
//        error_enable_syslog();


	/* Create UDP Mulciast/Unicast Socket */
	struct addrinfo hints, *res;
	char c_port[16];

        vxlan.port = VXLAN_PORT_BASE + subn;
	snprintf (c_port, sizeof (c_port), "%d", vxlan.port);

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
			
	if (getaddrinfo (mcast_caddr, c_port, &hints, &res) != 0)
		error_quit ("Invalid Multicast Address \"%s\"", mcast_caddr);

	if ((vxlan.udp_sock = socket (res->ai_family, 
				      res->ai_socktype,
				      res->ai_protocol)) < 0)
		err (EXIT_FAILURE, "can not create socket");


	memcpy (&vxlan.mcast_addr, res->ai_addr, res->ai_addrlen);

	freeaddrinfo (res);

	int off = 1, ttl = 255;
	struct sockaddr * saddr;
	struct ip_mreq mreq;
	struct ipv6_mreq mreq6;

	saddr = (struct sockaddr *) &vxlan.mcast_addr;
	
	switch (saddr->sa_family) {
	case AF_INET :
		mreq.imr_multiaddr = ((struct sockaddr_in *)&vxlan.mcast_addr)->sin_addr;
		mreq.imr_interface = getifaddr (vxlan_if_name);

		if (setsockopt (vxlan.udp_sock,
				IPPROTO_IP, 
				IP_ADD_MEMBERSHIP,
				(char *)&mreq, sizeof (mreq)) < 0)
			err (EXIT_FAILURE, "can not join multicast %s", mcast_caddr);

		if (setsockopt (vxlan.udp_sock,
				IPPROTO_IP,
				IP_MULTICAST_IF,
				(char *)&mreq.imr_interface, sizeof (mreq.imr_interface)) < 0)
			err (EXIT_FAILURE, "can not set multicast interface");

		if (setsockopt (vxlan.udp_sock,
				IPPROTO_IP,
				IP_MULTICAST_LOOP,
				(char *)&off, sizeof (off)) < 0)
			err (EXIT_FAILURE, "can not set off multicast loop");

		if (setsockopt (vxlan.udp_sock,
				IPPROTO_IP,
				IP_MULTICAST_TTL,
				(char *)&ttl, sizeof (ttl)) < 0)
			err (EXIT_FAILURE, "can not set ttl");

		break;

	case AF_INET6 :
		mreq6.ipv6mr_multiaddr = ((struct sockaddr_in6 *)&vxlan.mcast_addr)->sin6_addr;
		mreq6.ipv6mr_interface = if_nametoindex (vxlan_if_name);

		if (mreq6.ipv6mr_interface < 0)
			err (EXIT_FAILURE, "%s does not exist", vxlan_if_name);

		if (setsockopt (vxlan.udp_sock,
				IPPROTO_IPV6,
				IPV6_ADD_MEMBERSHIP,
				(char *)&mreq6, sizeof (mreq6)) < 0)
			err (EXIT_FAILURE, "can not join multicast %s", mcast_caddr);
		
		if (setsockopt (vxlan.udp_sock,
				IPPROTO_IPV6,
				IPV6_MULTICAST_IF,
				(char *)&mreq6.ipv6mr_interface, 
				sizeof (mreq6.ipv6mr_interface)) < 0)
			err (EXIT_FAILURE, "can not join multicast %s", mcast_caddr);

		if (setsockopt (vxlan.udp_sock,
				IPPROTO_IPV6,
				IPV6_MULTICAST_LOOP,
				(char *)&off, sizeof (off)) < 0)
			err (EXIT_FAILURE, "can not set off multicast loop");

		if (setsockopt (vxlan.udp_sock,
				IPPROTO_IPV6,
				IPV6_MULTICAST_HOPS,
				(char *)&ttl, sizeof (ttl)) < 0)
			err (EXIT_FAILURE, "can not set ttl");
		break;

	default :
		error_quit ("unkown protocol family");
	}

	/* Create vxlan tap interface socket */

        if (subn >= 4096 ) {
            error_quit ("Invalid subinterface number %u", subn);
        }


        saddr_in = (struct sockaddr_in *) &vxlan.mcast_addr;
        saddr_in->sin_port = htons(vxlan.port);

        (void)snprintf(tunifname, IFNAMSIZ, "%s%d", VXLAN_TUNNAME, subn);
	vxlan.tap_sock = tap_alloc(tunifname);
	tap_up(tunifname);
	vxlan.fdb = init_fdb ();


	if (d_flag > 0) {
		if (daemon (0, 0) < 0)
			err (EXIT_FAILURE, "failed to run as a daemon");
	}

	process_vxlan ();

	return -1;
}


void
process_vxlan (void)
{
	int max_sock, len;
	char buf[VXLAN_PACKET_BUF_LEN];
	fd_set fds;

	struct vxlan_hdr 	* vhdr;
	struct ether_header 	* ether;
	struct sockaddr_storage sa_str;
	socklen_t s_t = sizeof (sa_str);

	memset (buf, 0, sizeof (buf));

	max_sock = (vxlan.tap_sock > vxlan.udp_sock) ? vxlan.tap_sock : vxlan.udp_sock;

	while (1) {
		FD_ZERO (&fds);
		FD_SET (vxlan.udp_sock, &fds);
		FD_SET (vxlan.tap_sock, &fds);
		
		if (select (max_sock + 1, &fds, NULL, NULL, NULL) < 0)
			err (EXIT_FAILURE, "select failed");

		/* From Tap */
		if (FD_ISSET (vxlan.tap_sock, &fds)) {
			if ((len = read (vxlan.tap_sock, buf, sizeof (buf))) < 0) {
				error_warn("read from tap failed");
				continue;
			}
			send_etherflame_from_local_to_vxlan ((struct ether_header *)buf, len);
		}

		
		/* From Internet */
		if (FD_ISSET (vxlan.udp_sock, &fds)) {

			if ((len = recvfrom (vxlan.udp_sock, buf, sizeof (buf), 0, 
					     (struct sockaddr *)&sa_str, &s_t)) < 0) {
				error_warn ("read from udp socket failed");
				continue;
			}
			
			vhdr = (struct vxlan_hdr *) buf;
			if (CHECK_VNI (vhdr->vxlan_vni, vxlan.vni) < 0)	
				continue;

			ether = (struct ether_header *) (buf + sizeof (struct vxlan_hdr));
			process_fdb_etherflame_from_vxlan (ether, &sa_str);
			send_etherflame_from_vxlan_to_local (ether, 
							     len - sizeof (struct vxlan_hdr));
		}
	}

	return;
}


void 
debug_print_vhdr (struct vxlan_hdr * vhdr)
{
	printf ("vxlan header\n");
	printf ("Flag : %u\n", vhdr->vxlan_flags);
	printf ("VNI  : %u%u%u\n", vhdr->vxlan_vni[0], vhdr->vxlan_vni[1], vhdr->vxlan_vni[2]);
	printf ("\n");

	return;
}

void
debug_print_ether (struct ether_header * ether) 
{
	printf ("Mac\n");
	printf ("DST : %02x:%02x:%02x:%02x:%02x:%02x\n",
		ether->ether_dhost[0], ether->ether_dhost[1], 
		ether->ether_dhost[2], ether->ether_dhost[3], ether->ether_dhost[4], 
		ether->ether_dhost[5]);
	printf ("SRC : %02x:%02x:%02x:%02x:%02x:%02x\n",
		ether->ether_shost[0], ether->ether_shost[1], 
		ether->ether_shost[2], ether->ether_shost[3], ether->ether_shost[4],
		ether->ether_shost[5]);
	return;
}

