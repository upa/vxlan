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
#include <net/if.h>
#include <netinet/in.h>
#include <syslog.h>


#include "common.h"
#include "error.h"
#include "net.h"
#include "fdb.h"
#include "iftap.h"
#include "vxlan.h"


struct vxlan vxlan;
unsigned short port = 0;

void process_vxlan (void);

void debug_print_vhdr (struct vxlan_hdr * vhdr);
void debug_print_ether (struct ether_header * ether);


void
usage (void)
{
	printf ("Usage\n");
	printf ("\n");
	printf ("   vxlan -m [MCASTADDR] -i [INTERFACE] [VNI] [VNI]... ");
	printf ("\n");
	printf ("\t -m : Multicast Address(v4/v6)\n");
	printf ("\t -i : Multicast Interface\n");
	printf ("\t -n : Sub Port number (<4096 default 0)\n");
	printf ("\t -e : Print Error Massage to STDOUT\n");
	printf ("\t -c : Access List File\n");
	printf ("\t -d : Daemon Mode\n");
	printf ("\n");
}


int
main (int argc, char * argv[])
{
	int ch, n;
	int d_flag = 0, err_flag = 0;
        int subn = 0;

	extern int opterr;
	extern char * optarg;
	struct addrinfo hints, *res;
	struct in6_addr in6addr;

	char configfile[48] = "";
	char mcast_caddr[40] = "";
	char vxlan_if_name[IFNAMSIZ] = "";
	u_int8_t vnibuf[VXLAN_VNISIZE];

	memset (&vxlan, 0, sizeof (vxlan));

	while ((ch = getopt (argc, argv, "em:i:c:n:d")) != -1) {
		switch (ch) {
		case 'e' :
			err_flag = 1;
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
			vxlan.subnum = subn;
			
			break;

		case 'd' :
			d_flag = 1;
			break;

		case 'c' :
			strncpy (configfile, optarg, sizeof (configfile));
			break;

		default :
			usage ();
			return -1;
		}
	}

	if ((vxlan.vins_num = argc - optind) < 1) {
		usage ();
		error_quit ("Please Set VNI(s)");
	}

	if (d_flag > 0) {
		if (daemon (1, err_flag) < 0)
			err (EXIT_FAILURE, "failed to run as a daemon");
	}

	/* Create UDP Mulciast/Unicast Socket */

        vxlan.port = VXLAN_PORT_BASE + subn;

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
			
	if (getaddrinfo (mcast_caddr, NULL, &hints, &res) != 0) {
		usage ();
		error_quit ("Invalid Multicast Address \"%s\"", mcast_caddr);
	}

	if ((vxlan.udp_sock = socket (res->ai_family, 
				      res->ai_socktype,
				      res->ai_protocol)) < 0)
		err (EXIT_FAILURE, "can not create socket");

	memcpy (&vxlan.mcast_addr, res->ai_addr, res->ai_addrlen);
	
	freeaddrinfo (res);

	switch (((struct sockaddr *)&vxlan.mcast_addr)->sa_family) {
	case AF_INET :
		set_ipv4_multicast_join_and_iface (vxlan.udp_sock, 
						   ((struct sockaddr_in *)
						    &vxlan.mcast_addr)->sin_addr,
						   vxlan_if_name);
		set_ipv4_multicast_loop (vxlan.udp_sock, 0);
		set_ipv4_multicast_ttl (vxlan.udp_sock, VXLAN_MCAST_TTL);
		bind_ipv4_inaddrany (vxlan.udp_sock, vxlan.port);
		break;

	case AF_INET6 :
		ifaddr (AF_INET6, vxlan_if_name, &in6addr);

		set_ipv6_multicast_join_and_iface (vxlan.udp_sock,
						   ((struct sockaddr_in6 *)
						    &vxlan.mcast_addr)->sin6_addr,
						   vxlan_if_name);
		set_ipv6_multicast_loop (vxlan.udp_sock, 0);
		set_ipv6_multicast_ttl (vxlan.udp_sock, VXLAN_MCAST_TTL);
		bind_ipv6_inaddrany (vxlan.udp_sock, vxlan.port);

		break;

	default :
		error_quit ("unkown protocol family");
	}


	((struct sockaddr_in *)&vxlan.mcast_addr)->sin_port = htons (vxlan.port);
	

	/* Create vxlan tap interface instance(s) */

	init_hash (&vxlan.vins_tuple, VXLAN_VNISIZE);


	vxlan.vins = (struct vxlan_instance **) 
		malloc (sizeof (struct vxlan_instance) * vxlan.vins_num);

	for (n = 0; n < vxlan.vins_num; n++) {
		strtovni (argv[optind + n], vnibuf);
		vxlan.vins[n] = (struct vxlan_instance *) 
			malloc (sizeof (struct vxlan_instance *));
		vxlan.vins[n] = create_vxlan_instance (vnibuf, configfile);
		insert_hash (&vxlan.vins_tuple, vxlan.vins[n], vnibuf);
		init_vxlan_instance (vxlan.vins[n]);
	}


        /* Enable syslog */
	if (err_flag == 0) {
		openlog (VXLAN_LOGNAME, 
			 LOG_PID | LOG_CONS | LOG_PERROR, 
			 VXLAN_LOGFACILITY);
		error_enable_syslog();
		syslog (LOG_INFO, "vxlan start");
	}


	process_vxlan ();

	/* not reached */

	return -1;
}


void
process_vxlan (void)
{
	int len;
	char buf[VXLAN_PACKET_BUF_LEN];
	fd_set fds;

	struct vxlan_hdr 	* vhdr;
	struct ether_header 	* ether;
	struct sockaddr_storage sa_str;
	struct vxlan_instance 	* vins;
	socklen_t s_t = sizeof (sa_str);

	memset (buf, 0, sizeof (buf));

	while (1) {
		FD_ZERO (&fds);
		FD_SET (vxlan.udp_sock, &fds);
		
		if (select (vxlan.udp_sock + 1, &fds, NULL, NULL, NULL) < 0)
			err (EXIT_FAILURE, "select failed");

		/* From Internet */
		if ((len = recvfrom (vxlan.udp_sock, buf, sizeof (buf), 0, 
				     (struct sockaddr *)&sa_str, &s_t)) < 0) {
			error_warn ("read from udp socket failed");
			continue;
		}
			
		vhdr = (struct vxlan_hdr *) buf;
		if ((vins = search_hash (&vxlan.vins_tuple, vhdr->vxlan_vni)) == NULL) {
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

