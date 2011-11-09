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


#include "common.h"
#include "net.h"
#include "fdb.h"
#include "iftap.h"


struct vxlan vxlan;

void process_vxlan (void);

void debug_print_vhdr (struct vxlan_hdr * vhdr);
void debug_print_ether (struct ether_header * ether);

void
usage (void)
{
	printf ("Usage\n");
	printf ("\t -v : VXLAN Network Identifier (24bit Hex)\n");
	printf ("\t -m : Multicast Address\n");
	printf ("\t -i : Multicast Interface\n");
	printf ("\t -n : Sub-interface number\n");
	printf ("\t -d : Daemon Mode\n");
	printf ("\n");
}


int
main (int argc, char * argv[])
{
	int ch;
	int d_flag = 0;
        int sockopt;
        int subn;
	u_int32_t vni32;
	struct sockaddr_in * saddr_in;

	extern int opterr;	
	extern char * optarg;

	char mcast_if_name[IFNAMSIZ];
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
			
			(vni32 == LONG_MAX) ? err (EXIT_FAILURE, "strtol overflow") :
				(vni32 == LONG_MIN) ? 
				err (EXIT_FAILURE, "strtol underflow") :
				memcpy (vxlan.vni, &vni32, VXLAN_VNISIZE);

			break;

		case 'm' :
			if (optarg == NULL) {
				usage ();
				return -1;
			}

			if (inet_pton (AF_INET, optarg, &vxlan.mcast_addr) < 1) 
				err (EXIT_FAILURE, "invalid Mcast Address %s", optarg);

			saddr_in = (struct sockaddr_in *) &vxlan.mcast_saddr;
			saddr_in->sin_family = AF_INET;
			saddr_in->sin_addr = vxlan.mcast_addr;
			saddr_in->sin_port = htons (VXLAN_MCAST_PORT);
			
			break;

		case 'i' :
			if (optarg == NULL) {
				usage ();
				return -1;
			}
			strcpy (mcast_if_name, optarg);
			
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
        (void)snprintf(tunifname, IFNAMSIZ, "%s%d", VXLAN_TUNNAME, subn);

	vxlan.tap_sock = tap_alloc (tunifname);
	vxlan.udp_sock = udp_sock (VXLAN_PORT);
	vxlan.mst_send_sock = mcast_send_sock (VXLAN_MCAST_PORT, 
					       getifaddr (mcast_if_name));
	vxlan.mst_recv_sock = mcast_recv_sock (VXLAN_MCAST_PORT,
					       getifaddr (mcast_if_name), 
					       vxlan.mcast_addr);

        sockopt = 0;
        if ( 0 != setsockopt(vxlan.mst_send_sock, IPPROTO_IP, IP_MULTICAST_LOOP,
                             (void*)&sockopt, sizeof(sockopt)) ) {
            err ( EXIT_FAILURE, "failed to disable IP_MULTICAST_LOOP" );
        }

	tap_up (tunifname);
	init_hash (&vxlan.fdb);
	fdb_decrease_ttl_init ();

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
	struct sockaddr 	src_saddr;
	struct sockaddr_in 	* src_saddr_in;
	socklen_t peer_addr_len;

	memset (buf, 0, sizeof (buf));

	max_sock = (vxlan.tap_sock > vxlan.udp_sock) ? vxlan.tap_sock : vxlan.udp_sock;
	max_sock = (vxlan.mst_recv_sock > max_sock) ? vxlan.mst_recv_sock : max_sock;

	while (1) {
		FD_ZERO (&fds);
		FD_SET (vxlan.udp_sock, &fds);
		FD_SET (vxlan.tap_sock, &fds);
		FD_SET (vxlan.mst_recv_sock, &fds);
		
		if (select (max_sock + 1, &fds, NULL, NULL, NULL) < 0)
			err (EXIT_FAILURE, "select failed");

		/* From Tap */
		if (FD_ISSET (vxlan.tap_sock, &fds)) {
			if ((len = read (vxlan.tap_sock, buf, sizeof (buf))) < 0) {
				warn ("read from tap failed");
				continue;
			}
			send_etherflame_from_local_to_vxlan ((struct ether_header *)buf, len);
		}

		/* From Unicast UDP */ 
		if (FD_ISSET (vxlan.udp_sock, &fds)) {
			if ((len = recvfrom (vxlan.udp_sock, buf, sizeof (buf), 0, 
					     &src_saddr, &peer_addr_len)) < 0) {
				warn ("read from udp unicast socket failed");
				continue;
			}
			src_saddr_in = (struct sockaddr_in *) &src_saddr;

			vhdr = (struct vxlan_hdr *) buf;
			if (CHECK_VNI (vhdr->vxlan_vni, vxlan.vni) < 0)	continue;

			ether = (struct ether_header *) (buf + sizeof (struct vxlan_hdr));
			process_fdb_etherflame_from_vxlan (ether, &src_saddr_in->sin_addr);
			send_etherflame_from_vxlan_to_local (ether, 
							     len - sizeof (struct vxlan_hdr));
		}

		/* From Multicast */
		if (FD_ISSET (vxlan.mst_recv_sock, &fds)) {
			if ((len = recvfrom (vxlan.mst_recv_sock, buf, sizeof (buf), 0, 
					     &src_saddr, &peer_addr_len)) < 0) {
				warn ("read from udp multicast socket failed");
				continue;
			}
			src_saddr_in = (struct sockaddr_in *) &src_saddr;

			vhdr = (struct vxlan_hdr *) buf;
			if (CHECK_VNI (vhdr->vxlan_vni, vxlan.vni) < 0)
				continue;

			ether = (struct ether_header *) (buf + sizeof (struct vxlan_hdr));
			process_fdb_etherflame_from_vxlan (ether, &src_saddr_in->sin_addr);
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



void debug_print_ether (struct ether_header * ether);
