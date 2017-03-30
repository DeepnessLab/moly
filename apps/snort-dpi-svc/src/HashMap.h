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

/* Hash Map with pointer key support. */

typedef struct {
	void *key;
	void *data;
	UT_hash_handle hh;
} HashPtrObject;

typedef struct {
	HashPtrObject *map;
	UT_hash_handle *hh;
	HashPtrObject *iter;
} HashMapPtr;

typedef void (*PrintElement)(void *data);

HashMapPtr *hashmapptr_create();
void hashmapptr_destroy(HashMapPtr *map);
void *hashmapptr_get(HashMapPtr *map, void *key);
void hashmapptr_put(HashMapPtr *map, void *key, void *value);
int hashmapptr_remove(HashMapPtr *map, void *key);
int hashmapptr_size(HashMapPtr *map);
void hashmapptr_iterator_reset(HashMapPtr *map);
void *hashmapptr_iterator_next(HashMapPtr *map);
HashPtrObject *hashmapptr_iterator_next_entry(HashMapPtr *map);
void hashmapptr_print(HashMapPtr *map, PrintElement printFunc);

#endif /* HASHMAP_H_ */
