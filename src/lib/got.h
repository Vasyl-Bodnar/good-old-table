/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GOT_H_
#define GOT_H_

#include <stdint.h>
#include <string.h>

// Fairly certain this is a default on (x86) 64 bit systems anyway
#ifdef __SSE2__
#define GROUP_SIZE 16
#include <emmintrin.h>
#else
#define GROUP_SIZE 8
#endif

uint64_t power_of_two(uint64_t x);

#define calc_ht_size(len, key_size, val_size)                                  \
    (sizeof(HashTable) +                                                       \
     (power_of_two(len) + power_of_two(len) % GROUP_SIZE) * (key_size + 1) +   \
     (power_of_two(len) + power_of_two(len) % GROUP_SIZE) * val_size)

// For now we utilize this simple hash function.
uint64_t fnv1a_hash(uint8_t *input, size_t length);

typedef struct HashTable {
    size_t key_size;
    size_t val_size;
    size_t length;
    size_t capacity;
    uint8_t elems[]; // Note that this also includes control bytes
} HashTable;

// For return in `next_elem_ht` only
typedef struct Entry {
    void *key;
    void *value;
} Entry;

// Creates the table from memory.
// Must have enough for the table, use `calc_ht_size`.
HashTable *create_ht(void *memory, size_t len, size_t key_size,
                     size_t val_size);

// Creates a new table from memory, copying in all elements from old_ht.
// Does not change the old_ht.
HashTable *create_from_ht(void *memory, HashTable *old_ht, size_t len);

// Returns 0 on failure, if it is too full.
// If the key did not already exist, returns 1.
// If the key did exist, the value is replaced, and 2 is returned.
uint32_t put_elem_ht(HashTable *ht, void *key, void *value);

// Returns 0 if failed to find an element with that key.
// Otherwise returns a reference.
// The reference might become invalid if followed by a delete and put.
// Can be used as `exists` given that non-zero output implies existance.
void *get_elem_ht(HashTable *ht, void *key);

// Returns 0 if failed to find an element with that key.
// Otherwise 1.
uint32_t delete_elem_ht(HashTable *ht, void *key);

// Get the first live entry from the table starting at `idx`.
// Returns pointers to key and value.
// Both pointers will be set to 0 when finished (invalid `idx`).
// `idx` updates to after current entry.
Entry next_elem_ht(HashTable *ht, size_t *idx);

// Clears the table for reuse
void clear_ht(HashTable *ht);

#ifdef DYNAMIC_TABLE
#include <stdlib.h>

typedef struct DynHashTable {
    size_t key_size;
    size_t val_size;
    size_t length;
    size_t capacity;
    uint8_t elems[];
} DynHashTable;

// `create_ht` dynamic variant, mallocs
DynHashTable *create_dht(size_t len, size_t key_size, size_t val_size);

// `put_elem_ht` dynamic variant, mallocs and frees as necessary.
uint32_t put_elem_dht(DynHashTable **dht, void *key, void *value);

// `get_elem_ht` dynamic variant, identical behaviour.
void *get_elem_dht(DynHashTable *dht, void *key);

// `delete_elem_ht` dynamic variant, identical behaviour.
// Note that if table expands the tombstones will be deleted completely.
uint32_t delete_elem_dht(DynHashTable *dht, void *key);

// `next_elem_dht` dynamic variant, identical behaviour.
Entry next_elem_dht(DynHashTable *dht, size_t *idx);

// `clear_dht` dynamic variant, identical behaviour.
void clear_dht(DynHashTable *dht);

// Frees malloced table
void delete_dht(DynHashTable *dht);
#endif // DYNAMIC_TABLE

#endif // GOT_H_
