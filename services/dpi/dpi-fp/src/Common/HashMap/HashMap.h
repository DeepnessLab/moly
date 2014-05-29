/*
 * HashMap.h
 *
 *  Created on: Jan 12, 2011
 *      Author: yotamhc
 */

#ifndef HASHMAP_H_
#define HASHMAP_H_

#include "uthash.h"

typedef struct {
	int key;
	void *data;
	UT_hash_handle hh;
} HashObject;

typedef struct {
	HashObject *map;
	UT_hash_handle *hh;
	HashObject *iter;
} HashMap;

typedef void (*PrintElement)(void *data);

HashMap *hashmap_create();
void hashmap_destroy(HashMap *map);
void *hashmap_get(HashMap *map, int key);
void hashmap_put(HashMap *map, int key, void *value);
int hashmap_remove(HashMap *map, int key);
int hashmap_size(HashMap *map);
void hashmap_iterator_reset(HashMap *map);
void *hashmap_iterator_next(HashMap *map);
HashObject *hashmap_iterator_next_entry(HashMap *map);
void hashmap_print(HashMap *map, PrintElement printFunc);
void printPair(void *data);

#endif /* HASHMAP_H_ */
