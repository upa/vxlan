#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <net/if.h>
#include <sys/select.h>
#include <arpa/inet.h>


#include "common.h"
#include "net.h"
#include "iftap.h"

struct vxlan vxlan;

void init_vxlan (void);

void
usage (void)
{
	printf ("usage\n");
	printf ("\t -v : VXLAN Network Identifier (24bit Hex)\n");
	printf ("\t -m : Multicast Address\n");
	printf ("\t -i : Multicast Interface\n");
	printf ("\t -d : Daemon Mode\n");
	printf ("\n");
}


int
main (int argc, char * argv[])
{
	int ch;
	int d_flag = 0;
	u_int32_t vni32;
	struct sockaddr_in * saddr_in;

	extern int opterr;	
	extern char * optarg;

	char mcast_if_name[IFNAMSIZ];
	
	memset (&vxlan, 0, sizeof (vxlan));

	while ((ch = getopt (argc, argv, "v:m:i:d")) != -1) {
		switch (ch) {
		case 'v' :
			if (optarg == NULL) {
				usage ();
				return -1;
			}

			vni32 = strtol (optarg, NULL, 16);
			if (vni32 == LONG_MAX) {
				err (EXIT_FAILURE, "strtol overflow");
			} 
			
			(vni32 == LONG_MAX) ? err (EXIT_FAILURE, "strtol overflow") :
				(vni32 == LONG_MIN) ? err (EXIT_FAILURE, "strtol underflow") :
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
			saddr_in->sin_port = htons (VXLAN_PORT);
			
			break;

		case 'i' :
			if (optarg == NULL) {
				usage ();
				return -1;
			}
			strcpy (mcast_if_name, optarg);
			
			break;

		case 'd' :
			d_flag = 1;
			break;

		default :
			usage ();
			return -1;
		}
	}

	vxlan.tap_sock = tap_alloc (VXLAN_TUNNAME);
	vxlan.udp_sock = udp_sock (VXLAN_PORT);
	vxlan.mst_sock = mcast_sock (VXLAN_PORT, getifaddr (mcast_if_name), vxlan.mcast_addr);

	tap_up (VXLAN_TUNNAME);
	
	init_hash (&vxlan.fdb);

	if (d_flag > 0) {
		if (daemon (0, 0) < 0)
			err (EXIT_FAILURE, "failed to run as a daemon");
	}

	init_vxlan ();

	return -1;
}

void
init_vxlan (void)
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
	max_sock = (vxlan.mst_sock > max_sock) ? vxlan.mst_sock : max_sock;

	while (1) {
		FD_ZERO (&fds);
		FD_SET (vxlan.udp_sock, &fds);
		FD_SET (vxlan.mst_sock, &fds);
		FD_SET (vxlan.tap_sock, &fds);
		
		if (select (max_sock + 1, &fds, NULL, NULL, NULL) < 0)
			err (EXIT_FAILURE, "select failed");

		if (FD_ISSET (vxlan.tap_sock, &fds)) {
			if ((len = read (vxlan.tap_sock, buf, sizeof (buf))) < 0) {
				warn ("read from tap failed");
				continue;
			}
			send_etherflame_from_local_to_vxlan ((struct ether_header *) ether, len);
		}

		if (FD_ISSET (vxlan.udp_sock, &fds)) {
			if ((len = recvfrom (vxlan.udp_sock, buf, sizeof (buf), 0, 
					     &src_saddr, &peer_addr_len)) < 0) {
				warn ("read from udp unicast socket failed");
				continue;
			}
			src_saddr_in = (struct sockaddr_in *) &src_saddr;
			vhdr = (struct vxlan_hdr *) buf;
			/* vxlan headdr check (VNI) */

			ether = (struct ether_header *) (vhdr + 1);
			process_fdb_etherflame_from_vxlan (ether, &src_saddr_in->sin_addr);
			
			send_etherflame_from_vxlan_to_local (ether, 
							     len - sizeof (struct vxlan_hdr));
		}

		if (FD_ISSET (vxlan.mst_sock, &fds)) {
			if ((len = recvfrom (vxlan.mst_sock, buf, sizeof (buf), 0, 
					     &src_saddr, &peer_addr_len)) < 0) {
				warn ("read from udp multicast socket failed");
				continue;
			}
			src_saddr_in = (struct sockaddr_in *) &src_saddr;
			vhdr = (struct vxlan_hdr *) buf;
			/* vxlan headdr check (VNI) */

			ether = (struct ether_header *) (vhdr + 1);
			process_fdb_etherflame_from_vxlan (ether, &src_saddr_in->sin_addr);
			
			send_etherflame_from_vxlan_to_local (ether, 
							     len - sizeof (struct vxlan_hdr));
			
		}
	}

	return;
}
