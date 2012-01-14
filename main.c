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
	printf ("\t -m : Multicast Address\n");
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
					u_int8_t * vp = &vni32;
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

			if (inet_pton (AF_INET, optarg, &vxlan.mcast_addr) < 1) 
				err (EXIT_FAILURE, "invalid Mcast Address %s", optarg);

			saddr_in = (struct sockaddr_in *) &vxlan.mcast_saddr;
			saddr_in->sin_family = AF_INET;
			saddr_in->sin_addr = vxlan.mcast_addr;
			
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
        if ( subn >= 4096 ) {
            err (EXIT_FAILURE, "Invalid subinterface number %u", subn);
        }

        /* Enable syslog */
        error_enable_syslog();

        (void)snprintf(tunifname, IFNAMSIZ, "%s%d", VXLAN_TUNNAME, subn);
        vxlan.port = VXLAN_PORT_BASE + subn;
        saddr_in = (struct sockaddr_in *) &vxlan.mcast_saddr;
        saddr_in->sin_port = htons(vxlan.port);

	vxlan.tap_sock = tap_alloc(tunifname);
	vxlan.udp_sock = mcast_sock (vxlan.port, getifaddr (vxlan_if_name), vxlan.mcast_addr);

	tap_up(tunifname);
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
	socklen_t peer_addr_len;

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

		/* From Unicast UDP */ 
		if (FD_ISSET (vxlan.udp_sock, &fds)) {
			struct in_addr src_addr, dst_addr;
			struct msghdr msg;
			struct iovec iov[1];
			char cbuf[512];
			struct in_pktinfo * pktinfo;
			struct cmsghdr * cmsg;
			struct sockaddr_in sin;
			
			iov[0].iov_base = buf;
			iov[0].iov_len = sizeof (buf);
			
			msg.msg_name = &sin;
			msg.msg_namelen = sizeof (sin);
			msg.msg_iov = iov;
			msg.msg_iovlen = 1;
			msg.msg_control = cbuf;
			msg.msg_controllen = sizeof (cbuf);
			
			if (recvmsg (vxlan.udp_sock, &msg, 0) < 0) {
				error_warn ("read from udp socket failed");
				continue;
			}
			
			pktinfo = NULL;
			for (cmsg = CMSG_FIRSTHDR (&msg); 
			     cmsg != NULL; cmsg = CMSG_NXTHDR (&msg, cmsg)) {
				if (cmsg->cmsg_level == IPPROTO_IP &&
				    cmsg->cmsg_type == IP_PKTINFO) {
					pktinfo = (struct in_pktinfo *) CMSG_DATA (cmsg);
					dst_addr = pktinfo->ipi_addr;
					src_addr = sin.sin_addr;
					break;
				}
			}

			if (pktinfo == NULL) {
				error_warn ("No pktinfo failed");
				continue;
			}
			
			vhdr = (struct vxlan_hdr *) buf;
			if (CHECK_VNI (vhdr->vxlan_vni, vxlan.vni) < 0)	continue;

			ether = (struct ether_header *) (buf + sizeof (struct vxlan_hdr));
			process_fdb_etherflame_from_vxlan (ether, &src_addr);
			send_etherflame_from_vxlan_to_local (ether, 
							     len - sizeof (struct vxlan_hdr));
		}

		

#if 0
		if (FD_ISSET (vxlan.udp_sock, &fds)) {
			if ((len = recvfrom (vxlan.udp_sock, buf, sizeof (buf), 0, 
					     &src_saddr, &peer_addr_len)) < 0) {
				error_warn("read from udp unicast socket failed");
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
				error_warn("read from udp multicast socket failed");
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

#endif
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
