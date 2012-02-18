#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/select.h>
#include <net/if.h>

#include "net.h"
#include "fdb.h"
#include "error.h"
#include "vxlan.h"
#include "iftap.h"

void * process_vxlan_instance (void * param);


/* access list related*/

struct acl_entry {
        u_int8_t mac[ETH_ALEN];
        u_int8_t vni[VXLAN_VNISIZE];
};

int
strtoaclentry (char * str, struct acl_entry * e)
{
        char cvni[16];
        int mac[ETH_ALEN];


        if (sscanf (str, "%s %x:%x:%x:%x:%x:%x",
                    cvni, &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) < 7) {
                return -1;
        }

        struct acl_entry entry = {
                { mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] },
                {0, 0, 0},
        };

        strtovni (cvni, entry.vni);

        memcpy (e, &entry, sizeof (entry));

        return 1;
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
	FILE * cfp;
	char cbuf[16];
	char buf[1024];
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
	

	/* create out bound access list */
	init_hash (&vins->acl, ETH_ALEN);

	if (configfile == NULL)
		return vins;

	if ((cfp = fopen (configfile, "r")) == NULL)
		error_exit (-1, "can not open config file \"%s\"", configfile);
	
	while (fgets (buf, sizeof (buf), cfp)) {
		if (strtoaclentry (buf, &ae) < 0) 
			continue;
		
		if (CHECK_VNI (vins->vni, ae.vni) < 0)
			continue;

		e = (struct acl_entry *) malloc (sizeof (struct acl_entry));
		memcpy (e, &ae, sizeof (ae));
		insert_hash (&vins->acl, e, e->mac);
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
