#include "hashtable.h"
#include "util.h"
static unsigned long hash_str(const char *s) {
  const unsigned long FNV_PRIME = 1099511628211u;
  unsigned long hash = 1469598103934665603u;
  for (; *s; ++s) {
    hash ^= (unsigned char)(*s);
    hash *= FNV_PRIME;
  }
  return hash;
}

HashTable *ht_create(size_t size) {
  if (size < 8)
    size = 64;
  HashTable *ht = calloc(1, sizeof(HashTable));
  if (!ht)
    return NULL;
  ht->buckets = calloc(size, sizeof(Entry *));
  if (!ht->buckets) {
    free(ht);
    return NULL;
  }
  ht->size = size;
  ht->count = 0;
  return ht;
}

static Entry *entry_create(const char *key, int value) {
  Entry *e = calloc(1, sizeof(Entry));
  if (!e)
    return NULL;
  e->key = strdup(key);
  if (!e->key) {
    free(e);
    return NULL;
  }
  e->value = value;
  e->next = NULL;
  return e;
}

int ht_put(HashTable *ht, const char *key, int value) {
  if (!ht || !key)
    return -1;
  unsigned long h = hash_str(key);
  size_t idx = h % ht->size;
  Entry *cur = ht->buckets[idx];
  for (; cur; cur = cur->next) {
    if (strcmp(cur->key, key) == 0) {
      cur->value = value;
      return 0;
    }
  }
  Entry *e = entry_create(key, value);
  if (!e)
    return -1;
  e->next = ht->buckets[idx];
  ht->buckets[idx] = e;
  ht->count++;
  return 0;
}

int ht_get(HashTable *ht, const char *key) {
  if (!ht || !key)
    return -1;
  unsigned long h = hash_str(key);
  size_t idx = h % ht->size;
  for (Entry *cur = ht->buckets[idx]; cur; cur = cur->next) {
    if (strcmp(cur->key, key) == 0)
      return cur->value;
  }
  return -1;
}

int ht_remove(HashTable *ht, const char *key) {
  if (!ht || !key)
    return -1;
  unsigned long h = hash_str(key);
  size_t idx = h % ht->size;
  Entry *prev = NULL;
  for (Entry *cur = ht->buckets[idx]; cur; prev = cur, cur = cur->next) {
    if (strcmp(cur->key, key) == 0) {
      if (prev)
        prev->next = cur->next;
      else
        ht->buckets[idx] = cur->next;
      free(cur->key);
      free(cur);
      ht->count--;
      return 0;
    }
  }
  return -1;
}

void ht_each(HashTable *ht, ht_iter_cb cb, void *user) {
  if (!ht || !cb)
    return;
  for (size_t i = 0; i < ht->size; ++i) {
    for (Entry *cur = ht->buckets[i]; cur; cur = cur->next) {
      cb(cur->key, cur->value, user);
    }
  }
}

void ht_free(HashTable *ht) {
  if (!ht)
    return;
  for (size_t i = 0; i < ht->size; ++i) {
    Entry *cur = ht->buckets[i];
    while (cur) {
      Entry *n = cur->next;
      free(cur->key);
      free(cur);
      cur = n;
    }
  }
  free(ht->buckets);
  free(ht);
}
