#ifndef _FDB_H_
#define _FDB_H_

#include <pthread.h>
#include <netinet/in.h>
#include <net/ethernet.h>
#include <uthash.h>


#define FDB_DEFAULT_CACHE_MAX_TTL 10

/* key is MAC Address */
struct fdb_entry {
	/* uthash key */
	struct mackey {
		u_int8_t mac[ETH_ALEN];
	} mac;

	struct sockaddr_storage vtep_addr;
	int ttl;	

	UT_hash_handle hh;
};

struct fdb {
	struct fdb_entry * table;
	int fdb_max_ttl;
	pthread_t decrease_ttl_t;
	pthread_mutex_t mutex;
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
