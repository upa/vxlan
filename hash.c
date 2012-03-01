#include "hash.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


void
init_hash (struct hash * hash, int keylen)
{
	int n;
	
	memset (hash, 0, sizeof (struct hash));
	for (n = 0; n < HASH_TABLE_SIZE; n++) {
		hash->table[n].data = NULL;
		hash->table[n].next = NULL;
		pthread_mutex_init (&hash->mutex[n], NULL);
	}
	
	hash->keylen = keylen;
	hash->count = 0;

	return;
}

int
calculate_hash (void * key, int keylen)
{

	int len;
	u_int8_t * buf = (u_int8_t *) key;
	u_int32_t  sum = 0;
		
	if (key == NULL)
		return -1;

	for (len = keylen; len > 0; len--) {
		sum += *buf;
		buf++;
	}

	return sum % HASH_TABLE_SIZE;
}


int
compare_key (void * key1, void * key2, int keylen)
{
	int i;
	u_int8_t * kb1 = (u_int8_t *) key1, 
		 * kb2 = (u_int8_t *) key2;

	if (key1 == NULL || key2 == NULL) 
		return -1;
	
	for (i = 0; i < keylen; i++) {
		if (*kb1 != *kb2) 
			return -1;
		kb1++;
		kb2++;
	}

	return 0;
}

int
insert_hash (struct hash * hash, void * data, void * key)
{
	int hash_value;
	struct hashnode * node, * ptr, * prev;
	
	if ((hash_value = calculate_hash (key, hash->keylen)) < 0) 
		return -1;

	prev = &hash->table[hash_value];
	pthread_mutex_lock (&hash->mutex[hash_value]);

	for (ptr = prev->next; ptr != NULL; ptr = ptr->next) {
		if (compare_key (key, ptr->key, hash->keylen) == 0) {
			pthread_mutex_unlock (&hash->mutex[hash_value]);
			return -1;
		}
		prev = ptr;
	}
	
	node = (struct hashnode *) malloc (sizeof (struct hashnode));
	memset (node, 0, sizeof (struct hashnode));
	node->next = NULL;
	node->data = data;
	node->key  = (u_int8_t *) malloc (hash->keylen);
	memcpy (node->key, key, hash->keylen);
	prev->next = node;

	hash->count++;

	pthread_mutex_unlock (&hash->mutex[hash_value]);

	return 1;
}

void *
delete_hash (struct hash * hash, void * key)
{
	int hash_value;
	void * data;
	struct hashnode * ptr, * prev;

	if ((hash_value = calculate_hash (key, hash->keylen)) < 0) 
		return NULL;

	prev = &hash->table[hash_value];
	
	pthread_mutex_lock (&hash->mutex[hash_value]);	

	for (ptr = prev->next; ptr != NULL; ptr = ptr->next) {
		if (compare_key (key, ptr->key, hash->keylen) == 0)
			break;
		prev = ptr;
	}

	if (ptr == NULL) {
		pthread_mutex_unlock (&hash->mutex[hash_value]);	
		return NULL;
	}		

	prev->next = ptr->next;
	data = ptr->data;
	free (ptr->key);
	free (ptr);

	hash->count--;

	pthread_mutex_unlock (&hash->mutex[hash_value]);	

	return data;
}

void *
search_hash (struct hash * hash, void * key)
{
	int hash_value;
	struct hashnode * ptr;

	if ((hash_value = calculate_hash (key, hash->keylen)) < 0)
		return NULL;
		
	ptr = &hash->table[hash_value];

	pthread_mutex_lock (&hash->mutex[hash_value]);	

	for (ptr = ptr->next; ptr != NULL; ptr = ptr->next) {
		if (compare_key (key, ptr->key, hash->keylen) == 0) {
			pthread_mutex_unlock (&hash->mutex[hash_value]);	
			return ptr->data;
		}
	}
	     
	pthread_mutex_unlock (&hash->mutex[hash_value]);	

	return NULL;
}



#include <stdio.h>

void
print_all_hash (struct hash * hash)
{
	int n;
	struct hashnode	* ptr, * pptr;

	for (n = 0; n < HASH_TABLE_SIZE; n++) {
		ptr = &hash->table[n];
		if (ptr->next == NULL) printf	("table[%d] = NULL\n", n);
		else {
			printf ("table[%d] = ", n);
			for (pptr = ptr->next;
			     pptr != NULL;
			     pptr = pptr->next) {
				printf ("%u %s, ", *pptr->key, (char *) pptr->data);
			}
			printf ("\n");
		}
	}
}
