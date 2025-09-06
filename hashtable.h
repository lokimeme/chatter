#pragma once

#include <stddef.h>

typedef struct Entry {
  char *key; // username
  int value; // socket fd
  struct Entry *next;
} Entry;

typedef struct {
  Entry **buckets;
  size_t size;
  size_t count;
} HashTable;

// Create hash table with given bucket count
HashTable *ht_create(size_t size);
// Insert or replace key->value
int ht_put(HashTable *ht, const char *key, int value);
// Get value for key; returns -1 if not found
int ht_get(HashTable *ht, const char *key);
// Remove key; return 0 on success, -1 if not found
int ht_remove(HashTable *ht, const char *key);
// Iterate: call cb(key, value, user) for each element
typedef void (*ht_iter_cb)(const char *, int, void *);
void ht_each(HashTable *ht, ht_iter_cb cb, void *user);
// Free the table
void ht_free(HashTable *ht);

#endif
