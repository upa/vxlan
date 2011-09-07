#ifndef _FDB_H_
#define _FDB_H_

#include <netinet/in.h>
#include "common.h"

/* key of hash is mac address */
struct fdb_entry {
	struct sockaddr vtep_addr;
	int ttl;
};

#define FDB_CACHE_TTL 1200	/* sec */
#define FDB_DECREASE_TTL_INTERVAL 1

int fdb_add_entry (struct hash * fdb, u_int8_t * mac, struct in_addr vtep_addr);
int fdb_del_entry (struct hash * fdb, u_int8_t * mac);
struct fdb_entry * fdb_search_entry (struct hash * fdb, u_int8_t * mac);
struct sockaddr * fdb_search_vtep_addr (struct hash * fdb, u_int8_t * mac);

void fdb_decrease_ttl (int sig);



#endif /* _FDB_H_ */
