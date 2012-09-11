/* vxlanctl.c */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/un.h>


#include "common.h"

int create_unix_client_socket (char * domain);


void
usage (void)
{
	printf ("\n"
		"   Usage :\n"
		"\t  vxlanctl [commands]\n"
		"\n"
		"   commands:    (VNI is hex)\n"
		"   create      <VNI>      add vxlan interface\n"
		"   destroy     <VNI>      delete vxlan interface\n"
		"   mcast_addr  <ADDRESS>  set multicast address\n"
		"   mcast_iface <IF NAME>  set multicast interface\n"
		"\n"
		);

	return;
}

int
main (int argc, char * argv[])
{
	int n, sock;
	char cmdstr[512], resstr[512], buf[512];
	
	if (argc < 2) {
		usage ();
		return 0;
	}

	if (strcmp (argv[1], "-h") == 0 ||
	    strcmp (argv[1], "--help") == 0) {
		usage ();
		return 0;
	}

	memset (cmdstr, 0, sizeof (cmdstr));
	memset (resstr, 0, sizeof (resstr));
	
	for (n = 1; n < argc; n++) {
		strncpy (buf, cmdstr, sizeof (buf));
		snprintf (cmdstr, sizeof (cmdstr), "%s %s", buf, argv[n]);
	}

	sock = create_unix_client_socket (VXLAN_UNIX_DOMAIN);

	if (write (sock, cmdstr, strlen (cmdstr)) < 0) {
		perror ("write");
		shutdown (sock, SHUT_RDWR);
		close (sock);
	}
	
	if (read (sock, resstr, sizeof (resstr)) < 0) {
		perror ("read");
		shutdown (sock, SHUT_RDWR);
		close (sock);
	}
	
	printf ("%s", resstr);

	return 0;
}


int
create_unix_client_socket (char * domain)
{
	int sock;
	struct sockaddr_un saddru;
	
	memset (&saddru, 0, sizeof (saddru));
	saddru.sun_family = AF_UNIX;
	strncpy (saddru.sun_path, domain, UNIX_PATH_MAX);
	
	if ((sock = socket (AF_UNIX, SOCK_STREAM, 0)) < 0)
		err (EXIT_FAILURE, "can not create unix socket");
	
	if (connect (sock, (struct sockaddr *)&saddru, sizeof (saddru)) != 0)
		err (EXIT_FAILURE, "can not connect to unix socket \"%s\"", domain);
	
	return sock;
}
