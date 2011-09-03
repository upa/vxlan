#ifndef _HASH_H_
#define _HASH_H_

#include <sys/types.h>

#define HASH_TABLE_SIZE 7
#define HASH_KEY_LEN 	1	/* Byte */

struct hashnode {
	u_int8_t key[HASH_KEY_LEN];
	void * data;
	struct hashnode * next;
};

struct hash {
	struct hashnode table[HASH_TABLE_SIZE];	/* sentinel */
};

void init_hash (struct hash * hash);

int insert_hash (struct hash * hash, void * data, void * key);
int delete_hash (struct hash * hash, void * key);
void * search_hash (struct hash * hash, void * key);

void print_all_hash (struct hash * hash);
#endif _HASH_H_
