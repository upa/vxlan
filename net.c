#include "net.h"
#include "common.h"
#include "fdb.h"

#include <err.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <linux/if_ether.h>

int
udp_sock (int port)
{
	int sock;
	struct sockaddr_in saddr_in;
	
	if ((sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		err (EXIT_FAILURE, "can not create udp socket");
	
	saddr_in.sin_family = AF_INET;
	saddr_in.sin_port = htons (port);
	saddr_in.sin_addr.s_addr = INADDR_ANY;

	if (bind (sock, (struct sockaddr *)&saddr_in, sizeof (saddr_in)) < 0)
		err (EXIT_FAILURE, "can not bind udp socket");


	return sock;
}


