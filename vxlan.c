#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/select.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "net.h"
#include "fdb.h"
#include "error.h"
#include "vxlan.h"
#include "iftap.h"

void * process_vxlan_instance (void * param);


/* access list related type */

#define ACL_TYPE_MAC	0 
#define ACL_TYPE_IP4	1
#define ACL_TYPE_IP6	2
#define ACL_TYPE_RA	3
#define ACL_TYPE_RS	4

struct acl_entry {
	int type;
        u_int8_t vni[VXLAN_VNISIZE];

	union {
		u_int8_t mac[ETH_ALEN];
		struct in_addr addr4;
		struct in6_addr addr6;
	} term;
};
#define term_mac	term.mac
#define term_ip4	term.addr4
#define term_ip6	term.addr6


int
strtoaclentry (char * str, struct acl_entry * e)
{
	char type[16];
        char cvni[16];
	char addr[48];
        int mac[ETH_ALEN];
        struct acl_entry entry;
	
	if (sscanf (str, "%s %s %s", type, cvni, addr) < 3)
		return -1;

	if (type[0] == '#' || type[0] == ' ')	
		return -1;

        strtovni (cvni, entry.vni);

	if (strncmp (type, "mac", 3) == 0) {
		if (sscanf (addr, "%x:%x:%x:%x:%x:%x", &mac[0], &mac[1], 
			    &mac[2], &mac[3], &mac[4], &mac[5]) < 6)
			return -1;

		entry.type = ACL_TYPE_MAC;
		entry.term_mac[0] = mac[0];
		entry.term_mac[1] = mac[1];
		entry.term_mac[2] = mac[2];
		entry.term_mac[3] = mac[3];
		entry.term_mac[4] = mac[4];
		entry.term_mac[5] = mac[5];
		
		memcpy (e, &entry, sizeof (entry));
		return ACL_TYPE_MAC;

	} else if (strncmp (type, "ns", 2) == 0) {
		entry.type = ACL_TYPE_IP6;
		if (inet_pton (AF_INET6, addr, &entry.term_ip6) < 1)
			return -1;

		memcpy (e, &entry, sizeof (entry));
		return ACL_TYPE_IP6;

	} else if (strncmp (type, "arp", 3) == 0) {
		entry.type = ACL_TYPE_IP4;
		if (inet_pton (AF_INET, addr, &entry.term_ip4) < 1)
			return -1;

		memcpy (e, &entry, sizeof (entry));
		return ACL_TYPE_IP4;
	} else if (strncmp (type, "ra", 2) == 0) {
		return ACL_TYPE_RA;
	} else if (strncmp (type, "rs", 2) == 0) {
		return ACL_TYPE_RS;
	}
		
	return -1;
}



void
strtovni (char * str, u_int8_t * vni)
{
        u_int32_t vni32;
        char buf[9];

        if (snprintf (buf, sizeof (buf), "0x%s", str) < 0)
                error_quit ("invalid VNI \"%s\"", str);

        vni32 = strtol (buf, NULL, 0);

        if (vni32 == LONG_MAX || vni32 == LONG_MIN)
                err (EXIT_FAILURE, "invalid VNI %u", vni32);

        memcpy (vni, ((char *)&vni32) + 2, 1);
        memcpy (vni + 1, ((char *)&vni32) + 1, 1);
        memcpy (vni + 2, ((char *)&vni32), 1);

        return;
}



struct vxlan_instance * 
create_vxlan_instance (u_int8_t * vni, char * configfile)
{
	int type;
	char cbuf[16];
	char buf[1024];
	FILE * cfp;
	u_int32_t vni32;
	struct vxlan_instance * vins;
	struct acl_entry ae, * e;

	/* create socket and fdb */
	vins = (struct vxlan_instance *) malloc (sizeof (struct vxlan_instance));
	memset (vins, 0, sizeof (struct vxlan_instance));
	memcpy (vins->vni, vni, VXLAN_VNISIZE);

	snprintf (cbuf, 16, "0x%02x%02x%02x", 
		  vins->vni[0],vins->vni[1], vins->vni[2]);
	vni32 = strtol (cbuf, NULL, 0);
	snprintf (vins->vxlan_tap_name, IFNAMSIZ, "%s%d-%x", 
		  VXLAN_TUNNAME, vxlan.subnum, vni32);

	vins->fdb = init_fdb ();
	vins->tap_sock = tap_alloc (vins->vxlan_tap_name);
	

	/* create out bound MAC/ARP/ND/RA access list */
	init_hash (&vins->acl_mac, ETH_ALEN);
	init_hash (&vins->acl_ip4, sizeof (struct in_addr));
	init_hash (&vins->acl_ip6, sizeof (struct in6_addr));

	if (configfile == NULL || configfile[0] == '\0')
		return vins;

	if ((cfp = fopen (configfile, "r")) == NULL)
		err (EXIT_FAILURE, "can not open config file \"%s\"", configfile);
	
	while (fgets (buf, sizeof (buf), cfp)) {
		if ((type = strtoaclentry (buf, &ae)) < 0) 
			continue;
		
		if (CHECK_VNI (vins->vni, ae.vni) < 0)
			continue;

		e = (struct acl_entry *) malloc (sizeof (ae));
		memcpy (e, &ae, sizeof (ae));

		switch (type) {
		case ACL_TYPE_MAC :
			insert_hash (&vins->acl_mac, e, &e->term_mac);
			printf ("insert mac\n");
			break;
		case ACL_TYPE_IP4 :
			insert_hash (&vins->acl_ip4, e, &e->term_ip4);
			break;
		case ACL_TYPE_IP6 :
			insert_hash (&vins->acl_ip6, e, &e->term_ip6);
			printf ("insert ipv6\n");
			break;
		case ACL_TYPE_RA :
			vins->acl_mask |= ACL_MASK_RA;
			printf ("out bound RA Block\n");
			break;	
		case ACL_TYPE_RS :
			vins->acl_mask |= ACL_MASK_RS;
			printf ("out bound RS Block\n");
			break;
			
		default :
			continue;
		}
	}

	fclose (cfp);
	
	return vins;
}


void
init_vxlan_instance (struct vxlan_instance * vins)
{
	pthread_attr_t attr;

	tap_up (vins->vxlan_tap_name);	

	pthread_attr_init (&attr);
	pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
	pthread_create (&vins->tid, &attr, process_vxlan_instance, vins);

	return;
}


void *
process_vxlan_instance (void * param)
{
	int len;
	char buf[VXLAN_PACKET_BUF_LEN];
	fd_set fds;
	struct vxlan_instance * vins;

	vins = (struct vxlan_instance *) param;
	
#ifdef DEBUG
	printf ("vxlan instance\n");
	printf ("IFNAME : %s\n", vins->vxlan_tap_name);
	printf ("SOCKET : %d\n", vins->tap_sock);
#endif

	while (1) {
		FD_ZERO (&fds);
		FD_SET (vins->tap_sock, &fds);

		if (select (vins->tap_sock + 1, &fds, NULL, NULL, NULL) < 0)
			err (EXIT_FAILURE, "select failed : %s", vins->vxlan_tap_name);

		/* From Tap */
		if ((len = read (vins->tap_sock, buf, sizeof (buf))) < 0) {
			error_warn ("read from tap failed");
			continue;
		}

		send_etherflame_from_local_to_vxlan (vins, 
						     (struct ether_header * )buf,
						     len);
	}

	/* not reached */
	return NULL;
}
