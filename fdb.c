#include "fdb.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <syslog.h>

#define FDB_DECREASE_TTL_INTERVAL 1

void fdb_decrease_ttl_thread_init (struct fdb * fdb);
void * fdb_decrease_ttl_thread (void * param);


struct fdb * 
init_fdb (void)
{
	struct fdb * fdb;

	fdb = (struct fdb *) malloc (sizeof (struct fdb));
	fdb->table = NULL;
	fdb->fdb_max_ttl = FDB_DEFAULT_CACHE_MAX_TTL;
	pthread_mutex_init (&(fdb->mutex), NULL);
	
	fdb_decrease_ttl_thread_init (fdb);

	return fdb;
}

void
destroy_fdb (struct fdb * fdb)
{
	struct fdb_entry * entry, * tmp;

	pthread_cancel (fdb->decrease_ttl_t);
	
	HASH_ITER (hh, fdb->table, entry, tmp) {
		HASH_DEL (fdb->table, entry);
		free (entry);
	}
	
	return;
}


struct fdb_entry *
fdb_search_entry (struct fdb * fdb, u_int8_t * mac)
{
	struct fdb_entry * entry;

	HASH_FIND (hh, fdb->table, 
		   (struct makeyc *) mac,
		   sizeof (struct mackey),
		   entry);

	return entry;
}

int
fdb_add_entry (struct fdb * fdb, u_int8_t * mac, struct sockaddr_storage vtep_addr)
{
	struct fdb_entry * entry;
	
	entry = (struct fdb_entry *) malloc (sizeof (struct fdb_entry));
	memset (entry, 0, sizeof (struct fdb_entry));
	entry->vtep_addr = vtep_addr;
	entry->ttl = fdb->fdb_max_ttl;
	memcpy (&(entry->mac), mac, ETH_ALEN);

	HASH_ADD (hh, fdb->table, mac, sizeof (entry->mac), entry);

	return 0;
}

int
fdb_del_entry (struct fdb * fdb, u_int8_t * mac) 
{
	struct fdb_entry * entry;
	
	entry = fdb_search_entry (fdb, mac);

	if (entry == NULL) 
		return -1;

	HASH_DEL (fdb->table, entry);
	free (entry);

	return 0;
}


struct sockaddr *
fdb_search_vtep_addr (struct fdb * fdb, u_int8_t * mac)
{
	struct fdb_entry * entry;
	
	if ((entry = fdb_search_entry (fdb, mac)) == NULL)
		return NULL;

	return (struct sockaddr *) &entry->vtep_addr;
}


void 
fdb_decrease_ttl_thread_init (struct fdb * fdb)
{
	pthread_attr_t attr;
	
	pthread_attr_init (&attr);
	pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
	pthread_create (&fdb->decrease_ttl_t, &attr, fdb_decrease_ttl_thread, fdb);

	return;
}


void *
fdb_decrease_ttl_thread (void * param) 
{
	struct fdb * fdb = (struct fdb *) param;
	struct fdb_entry * entry, * tmp;	

	entry = tmp = NULL;

	while (1) {
		HASH_ITER (hh, fdb->table, entry, tmp) {
			entry->ttl--;
			if (entry->ttl <= 0) {
				pthread_mutex_lock (&(fdb->mutex));
				HASH_DEL (fdb->table, entry);
				free (entry);
				pthread_mutex_unlock (&(fdb->mutex));
			}
		}
		sleep (FDB_DECREASE_TTL_INTERVAL);
	}

	/* not reached */
	return NULL;
}
