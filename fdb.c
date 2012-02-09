#include "fdb.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define FDB_DECREASE_TTL_INTERVAL 1

void fdb_decrease_ttl_thread_init (struct fdb * fdb);
void * fdb_decrease_ttl_thread (void * param);


struct fdb * 
init_fdb (void)
{
	struct fdb * fdb;

	fdb = (struct fdb *) malloc (sizeof (struct fdb));
	init_hash (&fdb->fdb);
	fdb->fdb_max_ttl = FDB_DEFAULT_CACHE_MAX_TTL;
	
	fdb_decrease_ttl_thread_init (fdb);

	return fdb;
}

void
destroy_fdb (struct fdb * fdb)
{
	pthread_cancel (fdb->decrease_ttl_t);
	return;
}

int
fdb_add_entry (struct fdb * fdb, u_int8_t * mac, struct sockaddr_storage vtep_addr)
{
	struct fdb_entry * entry;
	
	entry = (struct fdb_entry *) malloc (sizeof (struct fdb_entry));
	memset (entry, 0, sizeof (struct fdb_entry));

	entry->vtep_addr = vtep_addr;
	entry->ttl = fdb->fdb_max_ttl;
	
	return insert_hash (&fdb->fdb, entry, mac);
}

int
fdb_del_entry (struct fdb * fdb, u_int8_t * mac) 
{
	struct fdb_entry * entry;
	
	if ((entry = delete_hash (&fdb->fdb, mac)) != NULL) {
		free (entry);
		return 1;
	} 

	return -1;
}


struct fdb_entry *
fdb_search_entry (struct fdb * fdb, u_int8_t * mac)
{
	return search_hash (&fdb->fdb, mac);
}

struct sockaddr *
fdb_search_vtep_addr (struct fdb * fdb, u_int8_t * mac)
{
	struct fdb_entry * entry;
	
	if ((entry = search_hash (&fdb->fdb, mac)) == NULL)
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
	int n;
	struct fdb * fdb = (struct fdb *) param;
	struct hashnode * ptr, * prev;
	struct fdb_entry * entry;

	struct hash * hash = &fdb->fdb;

	while (1) {
		for (n = 0; n < HASH_TABLE_SIZE; n++) {
			pthread_mutex_lock (&hash->mutex[n]);
			prev = &hash->table[n];
			for (ptr = hash->table[n].next; ptr != NULL; ptr = ptr->next) {
				entry = (struct fdb_entry *) ptr->data;
				entry->ttl--;
				if (entry->ttl <= 0) {
					prev->next = ptr->next;
					free (ptr);
					free (entry);
					ptr = prev;
				} else
					prev = ptr;

			}
			pthread_mutex_unlock (&hash->mutex[n]);
		}
		sleep (FDB_DECREASE_TTL_INTERVAL);
	}

	/* not reached */
	return NULL;
}
