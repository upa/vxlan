#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <limits.h>
#include <string.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <syslog.h>

#include "common.h"
#include "sockaddrmacro.h"
#include "error.h"
#include "net.h"
#include "fdb.h"
#include "iftap.h"
#include "vxlan.h"
#include "control.h"


struct vxlan vxlan;

void process_vxlan (void);

void debug_print_vhdr (struct vxlan_hdr * vhdr);
void debug_print_ether (struct ether_header * ether);


void
usage (void)
{
	printf ("\n"
		" Usage\n"
		"\n"
		"   vxland -m [MCASTADDR] -i [INTERFACE]"
		"\n"
		"\t -e : Print Error Massage to STDOUT\n"
		"\t -d : Daemon Mode\n"
		"\t -h : Print Usage (this message)\n"
		"\n"
		"   vxland is forwarding daemon. Please configure using vxlanctl.\n"
		"\n"
		);

	return;
}

void
cleanup (void)
{
	struct vxlan_instance * vins, * tmp;
	
	/* stop control thread */
	pthread_cancel (vxlan.control_tid);

	/* stop all vxlan instance */
	HASH_ITER (hh, vxlan.vins_table, vins, tmp) {
		destroy_vxlan_instance (vins);
	}

	/* close sockets */
	close (vxlan.udp_sock);
	close (vxlan.control_sock);
	
	return;
}

void 
sig_cleanup (int signal)
{
	cleanup ();
}


int
main (int argc, char * argv[])
{
	int ch;
	int d_flag = 0, err_flag = 0;

	extern int opterr;
	extern char * optarg;
	struct in_addr m_addr;

	memset (&vxlan, 0, sizeof (vxlan));

	while ((ch = getopt (argc, argv, "ehd")) != -1) {
		switch (ch) {
		case 'e' :
			err_flag = 1;
			break;

		case 'd' :
			d_flag = 1;
			break;

		case 'h' :
			usage ();
			return 0;
			
		default :
			usage ();
			return -1;
		}
	}

	if (d_flag > 0) {
		if (daemon (1, err_flag) < 0)
			err (EXIT_FAILURE, "failed to run as a daemon");
	}

	/* Create UDP Mulciast Socket */

	vxlan.udp_sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        vxlan.port = VXLAN_PORT_BASE;
	inet_pton (AF_INET, VXLAN_DEFAULT_MCAST, &m_addr);
	FILL_SOCKADDR (AF_INET, &vxlan.mcast_addr, m_addr);
	((struct sockaddr_in *)&vxlan.mcast_addr)->sin_port = htons (vxlan.port);

	switch (EXTRACT_FAMILY (vxlan.mcast_addr)) {
	case AF_INET :
		set_ipv4_mcast_addr (vxlan.udp_sock, m_addr);
		bind_ipv4_inaddrany (vxlan.udp_sock, vxlan.port);
		set_ipv4_multicast_loop (vxlan.udp_sock, 0);
		set_ipv4_multicast_ttl (vxlan.udp_sock, VXLAN_MCAST_TTL);
		break;

	case AF_INET6 :
		bind_ipv6_inaddrany (vxlan.udp_sock, vxlan.port);
		break;

	default :
		error_quit ("unkown protocol family");
	}

	/* Start Control Thread */
	init_vxlan_control ();

        /* Enable syslog */
	if (err_flag == 0) {
		openlog (VXLAN_LOGNAME, 
			 LOG_CONS | LOG_PERROR, 
			 VXLAN_LOGFACILITY);
		error_enable_syslog();
		syslog (LOG_INFO, "vxlan start");
	}


	/* set signal handler */
	if (atexit (cleanup) < 0)
		err (EXIT_FAILURE, "failed to register exit hook");

	/*
	if (signal (SIGINT, sig_cleanup) == SIG_ERR)
		err (EXIT_FAILURE, "failed to register SIGINT hook");
	*/

	process_vxlan ();


	return 0;
}


void
process_vxlan (void)
{
	int len;
	char buf[VXLAN_PACKET_BUF_LEN];

	struct vxlan_hdr 	* vhdr;
	struct ether_header 	* ether;
	struct sockaddr_storage sa_str;
	struct vxlan_instance 	* vins;
	socklen_t s_t = sizeof (sa_str);

	memset (buf, 0, sizeof (buf));

	/* From Internet */
	while (1) {
		memset (&sa_str, 0, sizeof (sa_str));
		if ((len = recvfrom (vxlan.udp_sock, buf, sizeof (buf), 0,	
				     (struct sockaddr *)&sa_str, &s_t)) < 0) 
			continue;

		vhdr = (struct vxlan_hdr *) buf;
		if ((vins = search_vxlan_instance (vhdr->vxlan_vni)) == NULL) {
			error_warn ("invalid VNI %02x%02x%02x", 
				    vhdr->vxlan_vni[0], 
				    vhdr->vxlan_vni[1], 
				    vhdr->vxlan_vni[2]);
			continue;
		}
			
		ether = (struct ether_header *) (buf + sizeof (struct vxlan_hdr));
		process_fdb_etherflame_from_vxlan (vins, ether, &sa_str);
		send_etherflame_from_vxlan_to_local (vins, ether, 
						     len - sizeof (struct vxlan_hdr));

	}

	return;
}


void 
debug_print_vhdr (struct vxlan_hdr * vhdr)
{
	printf ("vxlan header\n");
	printf ("Flag : %u\n", vhdr->vxlan_flags);
	printf ("VNI  : %u%u%u\n", 
		vhdr->vxlan_vni[0], 
		vhdr->vxlan_vni[1], 
		vhdr->vxlan_vni[2]);
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

