#include "hash.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>


void
init_hash (struct hash * hash)
{
	int n;

	memset (hash, 0, sizeof (struct hash));
	for (n = 0; n < HASH_TABLE_SIZE; n++) {
		hash->table[n].data = NULL;
		hash->table[n].next = NULL;
		pthread_mutex_init (&hash->mutex[n], NULL);
	}
	
	return;
}

int
calculate_hash (void * key)
{

	int len;
	u_int8_t * buf = (u_int8_t *) key;
	u_int32_t  sum = 0;
		
	if (key == NULL)
		return -1;

	for (len = HASH_KEY_LEN; len > 0; len--) {
		sum += *buf;
		buf++;
	}

	return sum % HASH_TABLE_SIZE;
}


int
compare_key (void * key1, void * key2)
{
	int i;
	u_int8_t * kb1 = (u_int8_t *) key1, 
		 * kb2 = (u_int8_t *) key2;

	if (key1 == NULL || key2 == NULL) 
		return -1;
	
	for (i = 0; i < HASH_KEY_LEN; i++) {
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
	
	if ((hash_value = calculate_hash (key)) < 0) 
		return -1;

	prev = &hash->table[hash_value];
	pthread_mutex_lock (&hash->mutex[hash_value]);

	for (ptr = prev->next; ptr != NULL; ptr = ptr->next) {
		if (compare_key (key, ptr->key) == 0) {
			pthread_mutex_unlock (&hash->mutex[hash_value]);
			return -1;
		}
		prev = ptr;
	}
	
	node = (struct hashnode *) malloc (sizeof (struct hashnode));
	memset (node, 0, sizeof (struct hashnode));
	node->next = NULL;
	node->data = data;
	memcpy (node->key, key, HASH_KEY_LEN);
	prev->next = node;

	pthread_mutex_unlock (&hash->mutex[hash_value]);

	return 1;
}

int
delete_hash (struct hash * hash, void * key)
{
	int hash_value;
	struct hashnode * ptr, * prev;

	if ((hash_value = calculate_hash (key)) < 0) 
		return -1;

	prev = &hash->table[hash_value];
	
	pthread_mutex_lock (&hash->mutex[hash_value]);	

	for (ptr = prev->next; ptr != NULL; ptr = ptr->next) {
		if (compare_key (key, ptr->key) == 0)
			break;
		prev = ptr;
	}

	if (ptr == NULL) {
		pthread_mutex_unlock (&hash->mutex[hash_value]);	
		return -1;
	}		

	prev->next = ptr->next;
	free (ptr);

	pthread_mutex_unlock (&hash->mutex[hash_value]);	

	return 0;
}

void *
search_hash (struct hash * hash, void * key)
{
	int hash_value;
	struct hashnode * ptr;

	if ((hash_value = calculate_hash (key))	< 0)
		return NULL;
		
	ptr = &hash->table[hash_value];

	pthread_mutex_lock (&hash->mutex[hash_value]);	

	for (ptr = ptr->next; ptr != NULL; ptr = ptr->next) {
		if (compare_key (key, ptr->key) == 0) {
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
