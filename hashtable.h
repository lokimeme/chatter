#pragma once

#include <stddef.h>

//Used general header needed for hashtables using structs to set up hashtables
typedef struct Entry {
  char *key;
  int value;
  struct Entry *next;
} Entry;

typedef struct {
  Entry **buckets;
  size_t size;
  size_t count;
} HashTable;

HashTable *ht_create(size_t size);
int ht_put(HashTable *ht, const char *key, int value);
int ht_get(HashTable *ht, const char *key);
int ht_remove(HashTable *ht, const char *key);
typedef void (*ht_iter_cb)(const char *, int, void *);
void ht_each(HashTable *ht, ht_iter_cb cb, void *user);
void ht_free(HashTable *ht);
