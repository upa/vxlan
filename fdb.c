#include "fdb.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

int
fdb_add_entry (struct hash * fdb, u_int8_t * mac, struct in_addr vtep)
{
	struct fdb_entry * entry;
	struct sockaddr_in saddr_in;
	
	entry = (struct fdb_entry *) malloc (sizeof (struct fdb_entry));
	memset (entry, 0, sizeof (struct fdb_entry));
	
	saddr_in.sin_family = AF_INET;
	saddr_in.sin_port = htons (vxlan.port);
	saddr_in.sin_addr = vtep;

	memcpy (&entry->vtep_addr, &saddr_in, sizeof (saddr_in));
	entry->ttl = FDB_CACHE_TTL;
	
	return insert_hash (fdb, entry, mac);
}

int
fdb_del_entry (struct hash * fdb, u_int8_t * mac) 
{
	struct fdb_entry * entry;
	
	if ((entry = delete_hash (fdb, mac)) != NULL) {
		free (entry);
		return 1;
	} 

	return -1;
}

struct sockaddr *
fdb_search_vtep_addr (struct hash * fdb, u_int8_t * mac)
{
	struct fdb_entry * entry;
	
	if ((entry = search_hash (fdb, mac)) == NULL)
		return NULL;
	
	return &entry->vtep_addr;
}


void
fdb_decrease_ttl (int sig)
{
	int n;
	struct hash * fdb = &vxlan.fdb;
	struct hashnode * ptr, * prev;
	struct fdb_entry * entry;
	
	if (sig != SIGALRM) {
		warn ("unexpected signal %d", sig);
		return;
	}

	for (n = 0; n < HASH_TABLE_SIZE; n++) {
		pthread_mutex_lock (&fdb->mutex[n]);
		prev = &fdb->table[n];
		for (ptr = fdb->table[n].next; ptr != NULL; ptr = ptr->next) {
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
		pthread_mutex_unlock (&fdb->mutex[n]);
	}

	return;
}
