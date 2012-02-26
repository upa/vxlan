#ifndef _FDB_H_
#define _FDB_H_

#include <pthread.h>
#include <netinet/in.h>
#include <net/ethernet.h>

#include "hash.h"

#define FDB_DEFAULT_CACHE_MAX_TTL 1200

/* key is MAC Address */
struct fdb_entry {
	u_int8_t mac[ETH_ALEN];
	struct sockaddr_storage vtep_addr;
	int ttl;
};

struct fdb {
	struct hash fdb;
	int fdb_max_ttl;
	pthread_t decrease_ttl_t;
};


/* prototypes */
struct fdb * init_fdb (void);
void destroy_fdb (struct fdb * fdb);

int fdb_add_entry (struct fdb * fdb, u_int8_t * mac, struct sockaddr_storage vtep_addr);
int fdb_del_entry (struct fdb * fdb, u_int8_t * mac);
struct sockaddr * fdb_search_vtep_addr (struct fdb * fdb, u_int8_t * mac);
struct fdb_entry * fdb_search_entry (struct fdb * fdb, u_int8_t * mac);

void fdb_decrease_ttl_thread_init (struct fdb * fdb);


#endif
