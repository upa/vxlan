#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/un.h>


#include "error.h"
#include "common.h"
#include "control.h"
#include "net.h"
#include "vxlan.h"


void exec_command_invalid (char * str, int sock);
void exec_command_vni_create (char * str, int sock);
void exec_command_vni_destroy (char * str, int sock);
void exec_command_acl (char * str, int sock);

void (* exec_command_func[]) (char * str, int sock) =
{
	exec_command_invalid,
	exec_command_vni_create,
	exec_command_vni_destroy,
	exec_command_acl
};



/* strtoaclentry related */
#define ACLCMD_VNI	0
#define ACLCMD_TYPE	1
#define ACLCMD_ACT	2
#define ACLCMD_OPT	3

int
strtoaclentry (char * str, struct acl_entry * e)
{
	int argc;
	char * c;
	char * args[CONTROL_ARG_MAX];
	int mac[ETH_ALEN];
	struct acl_entry entry;

	memset (&entry, 0, sizeof (entry));
	memset (args, 0, sizeof (args));

	/* parse */
	for (argc = 0, c = str; *c == ' '; c++);
	while (*c && argc < CONTROL_ARG_MAX) {
		args[argc++] = c;
		while (*c && *c > ' ') c++;
		while (*c && *c <= ' ') *c++ = '\0';
	}

        strtovni (args[ACLCMD_VNI], entry.vni);


        if (strncmp (args[ACLCMD_TYPE], "mac", 3) == 0) {
		/* MAC Address Filter */
		if (argc < 4)
			return -1;

                if (sscanf (args[ACLCMD_OPT], "%x:%x:%x:%x:%x:%x", &mac[0], &mac[1],
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


        } else if (strncmp (args[ACLCMD_TYPE], "ns", 2) == 0) {
		/* IPv6 Neighbor Solicit Filter */
		if (argc < 4)
			return -1;

                entry.type = ACL_TYPE_IP6;
                if (inet_pton (AF_INET6, args[ACLCMD_OPT], &entry.term_ip6) < 1)
                        return -1;
		
                memcpy (e, &entry, sizeof (entry));


        } else if (strncmp (args[ACLCMD_TYPE], "arp", 3) == 0) {
		/* ARP Filter */

		if (argc < 4)
			return -1;

                entry.type = ACL_TYPE_IP4;
                if (inet_pton (AF_INET, args[ACLCMD_OPT], &entry.term_ip4) < 1)
                        return -1;

                memcpy (e, &entry, sizeof (entry));

        } else if (strncmp (args[ACLCMD_TYPE], "ra", 2)) {
		/* ARP Filter */

		entry.type = ACL_TYPE_RA;
		memcpy (e, &entry, sizeof (entry));


	} else if (strncmp (args[ACLCMD_TYPE], "rs", 2)) {
		/* Router Solicit Filter */

		entry.type = ACL_TYPE_RS;
		memcpy (e, &entry, sizeof (entry));
	}

#define STRTOACLENTRY_INSTALL	1
#define STRTOACLENTRY_UNINSTALL	2
	if (strncmp (args[ACLCMD_ACT], "deny", 4) == 0)
		return STRTOACLENTRY_INSTALL;

	if (strncmp (args[ACLCMD_ACT], "permit", 6) == 0)
		return STRTOACLENTRY_UNINSTALL;


	return -1;
}



int
install_acl_entry (struct vxlan_instance * vins, struct acl_entry ae)
{
        struct acl_entry * e;

        if (vins == NULL)
                return -1;

        if (CHECK_VNI (vins->vni, ae.vni) < 0)
                return -1;

        e = (struct acl_entry *) malloc (sizeof (struct acl_entry));
        memset (e, 0, sizeof (struct acl_entry));
        memcpy (e, &ae, sizeof (ae));

        switch (e->type) {
        case ACL_TYPE_MAC :
                insert_hash (&vins->acl_mac, e, &e->term_mac);
                break;
        case ACL_TYPE_IP4 :
                insert_hash (&vins->acl_ip4, e, &e->term_ip4);
                break;
        case ACL_TYPE_IP6 :
                insert_hash (&vins->acl_ip6, e, &e->term_ip6);
                break;
        case ACL_TYPE_RA :
                vins->acl_mask |= ACL_MASK_RA;
                break;
        case ACL_TYPE_RS :
                vins->acl_mask |= ACL_MASK_RS;
                break;

        default :
                return -1;
        }

        return 1;
}

int
uninstall_acl_entry (struct vxlan_instance * vins, struct acl_entry ae)
{
        struct acl_entry * e;

        if (vins == NULL)
                return -1;

        if (CHECK_VNI (vins->vni, ae.vni) < 0)
                return -1;

        e = (struct acl_entry *) malloc (sizeof (struct acl_entry));
        memset (e, 0, sizeof (struct acl_entry));
        memcpy (e, &ae, sizeof (ae));

        switch (e->type) {
        case ACL_TYPE_MAC :
                delete_hash (&vins->acl_mac, &e->term_mac);
                break;
        case ACL_TYPE_IP4 :
                delete_hash (&vins->acl_ip4, &e->term_ip4);
                break;
        case ACL_TYPE_IP6 :
                delete_hash (&vins->acl_ip6, &e->term_ip6);
                break;
        case ACL_TYPE_RA :
                vins->acl_mask ^= ACL_MASK_RA;
                break;
        case ACL_TYPE_RS :
                vins->acl_mask ^= ACL_MASK_RS;
                break;
        default :
                return -1;
        }

        return 1;
}




void * process_vxlan_control (void * param);
int create_unix_server_socket (char * domain);
int strtocmdtype (char * str);

void
init_vxlan_control (void)
{
	pthread_attr_t attr;

        pthread_attr_init (&attr);
        pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
        pthread_create (&vxlan.control_tid, &attr, process_vxlan_control, NULL);

        return;
}

void *
process_vxlan_control (void * param)
{
	char * c;
	int socket, accept_socket;
	char buf[CONTROL_MSG_BUF_LEN];
	fd_set fds;
	
	socket = create_unix_server_socket (VXLAN_UNIX_DOMAIN);
	vxlan.control_sock = socket;

	listen (socket, 1);

	while (1) {
		memset (buf, 0, sizeof (buf));

		FD_ZERO (&fds);
		FD_SET (socket, &fds);
		pselect (socket + 1, &fds, NULL, NULL, NULL, NULL);
		
		if (!FD_ISSET (socket, &fds))
			break;

		accept_socket = accept (socket, NULL, 0);

		if (read (accept_socket, buf, sizeof (buf)) < 0) {
			warn ("read(2) faild : control socket");
			shutdown (accept_socket, 1);
			close (accept_socket);
			continue;
		}

		for (c = buf; *c == ' '; c++);

		exec_command_func[strtocmdtype (c)] (c, accept_socket);

		if (shutdown (accept_socket, SHUT_RDWR) != 0) {
			error_warn ("%s : shutdown : %s", __func__, strerror (errno));
		}

		if (close (accept_socket) != 0) {
			error_warn ("%s : close : %s", __func__, strerror (errno));	
		}
	}

	shutdown (vxlan.control_sock, SHUT_RDWR);
        if (close (vxlan.control_sock) < 0)
                error_warn ("%s : close control socket failed : %s", strerror (errno));
	
	/* not reached */
	return NULL;
}


int
strtocmdtype (char * str)
{
	if (strncmp (str, "create", 6) == 0) 
		return COMMAND_TYPE_VNI_CREATE;

	if (strncmp (str, "destroy", 7) == 0)
		return COMMAND_TYPE_VNI_DESTROY;		

	if (strncmp (str, "acl", 3) == 0)
		return COMMAND_TYPE_ACL;

	return COMMAND_TYPE_INVALID;
}


void
strtocontrol (char * str)
{
	char type[16];
	char args[512];
	
	if (sscanf (str, "%s %s", type, args) < 2) {
		error_warn ("invalid control message \"%s\"", str);
		return;
	}

	return;
}



void
exec_command_invalid (char * str, int sock)
{
	write (sock, CONTROL_ERRMSG, sizeof (CONTROL_ERRMSG));
	return;
}

void
exec_command_vni_create (char * str, int sock)
{
	u_int8_t vnibuf[3];
	char cmd[CONTROL_CMD_BUF_LEN];
	char vni[CONTROL_CMD_BUF_LEN];
	struct vxlan_instance * vins;
	
	if (sscanf (str, "%s %s", cmd, vni) < 2) {
		write (sock, CONTROL_ERRMSG, sizeof (CONTROL_ERRMSG));
		return;
	}
	
	strtovni (vni, vnibuf);

	if ((vins = search_hash (&vxlan.vins_tuple, vnibuf)) != NULL) {
		char errbuf[] = "this vxlan instance exists\n";
		write (sock, errbuf, strlen (errbuf));
		return;
	}
	
	vins = create_vxlan_instance (vnibuf);
	insert_hash (&vxlan.vins_tuple, vins, vnibuf);
	init_vxlan_instance (vins);
	vxlan.vins_num++;

	char msgbuf[] = "created\n";
	write (sock, msgbuf, strlen (msgbuf));
	return;
}


void
exec_command_vni_destroy (char * str, int sock)
{
	u_int8_t vnibuf[3];
	char cmd[CONTROL_CMD_BUF_LEN];
	char vni[CONTROL_CMD_BUF_LEN];
	struct vxlan_instance * vins;
	
	if (sscanf (str, "%s %s", cmd, vni) < 2) {
		write (sock, CONTROL_ERRMSG, sizeof (CONTROL_ERRMSG));
		return;
	}
	
	strtovni (vni, vnibuf);
	if ((vins = search_hash (&vxlan.vins_tuple, vnibuf)) == NULL) {
		char errbuf[] = "this vxlan instance does not exists\n";
		write (sock, errbuf, strlen (errbuf));
		return;
	}
	
	if (destroy_vxlan_instance (vins) < 0) {
		char errbuf[] = "Delete vxlan instance failed\n";
		write (sock, errbuf, strlen (errbuf));

	} else {
		char errbuf[] = "Deleted\n";
		write (sock, errbuf, strlen (errbuf));
	}

	return;	
}


void
exec_command_acl (char * str, int sock)
{

	int n;
	struct acl_entry ae;

	struct vxlan_instance * vins;
	
	if ((n = strtoaclentry (str + 4, &ae)) < 0) {
		char errbuf[] = "inavlid acl entry\n";
		write (sock, errbuf, strlen (errbuf));
		return;
	}
		
	if ((vins = search_hash (&vxlan.vins_tuple, ae.vni)) == NULL) {
		char errbuf[] = "this vni instance does not exist\n";
		write (sock, errbuf, strlen (errbuf));
		return;
	}

	if (n == STRTOACLENTRY_INSTALL) {
		if (install_acl_entry (vins, ae) < 0) {
			char errbuf[] = "install access list failed\n";
			write (sock, errbuf, strlen (errbuf));
			return;
		} else {
			char errbuf[] = "access list installed\n";
			write (sock, errbuf, strlen (errbuf));
			return;
		}
	}

	if (n == STRTOACLENTRY_UNINSTALL) {
		if (uninstall_acl_entry (vins, ae) < 0) {
			char errbuf[] = "uninstall access list failed\n";
			write (sock, errbuf, strlen (errbuf));
			return;
		} else {
			char errbuf[] = "access list uninstalled\n";
			write (sock, errbuf, strlen (errbuf));
			return;
		}
	}

	return;
}

/*

void
exec_command_acl_del (char * str, int sock)
{

	struct acl_entry ae;
	struct vxlan_instance * vins;
	
	if (strtoaclentry (str + 7, &ae) < 0) {
		char errbuf[] = "inavlid acl entry\n";
		write (sock, errbuf, strlen (errbuf));
		return;
	}
		
	if ((vins = search_hash (&vxlan.vins_tuple, ae.vni)) == NULL) {
		char errbuf[] = "this vni instance does not exist\n";
		write (sock, errbuf, strlen (errbuf));
		return;
	}

	if (uninstall_acl_entry (vins, ae) < 0) {
		char errbuf[] = "uninstall access list failed\n";
		write (sock, errbuf, strlen (errbuf));
		return;
	}

	char errbuf[] = "access list uninstalled\n";
	write (sock, errbuf, strlen (errbuf));
	return;
	
}

*/




int
create_unix_server_socket (char * domain)
{
	int sock;
	struct sockaddr_un saddru;
	
	memset (&saddru, 0, sizeof (saddru));
	saddru.sun_family = AF_UNIX;
	strncpy (saddru.sun_path, domain, UNIX_PATH_MAX);

	if ((sock = socket (AF_UNIX, SOCK_STREAM, 0)) < 0)
		err (EXIT_FAILURE, "can not create unix socket");
	
	if (bind (sock, (struct sockaddr *)&saddru, sizeof (saddru)) < 0)  {
		printf ("%s exists\n", VXLAN_UNIX_DOMAIN);
		exit (-1);
	}

        return sock;
}
	
