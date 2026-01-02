/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GOT_H_
#define GOT_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GROUP_SIZE 8

uint64_t power_of_two(uint64_t x);

#define calc_ht_size(len, key_size, val_size)                                  \
    (sizeof(HashTable) + power_of_two(len) * key_size +                        \
     power_of_two(len) * val_size + power_of_two(len) +                        \
     (power_of_two(len) % GROUP_SIZE))

// For now we utilize this simple hash function.
uint64_t fnv1a_hash(uint8_t *input, uint64_t length);

typedef struct HashTable {
    uint32_t key_size;
    uint32_t val_size;
    uint64_t length;
    uint64_t capacity;
    uint8_t elems[]; // Note that this also includes control bytes
} HashTable;

// For return in `next_elem_ht` only
typedef struct Entry {
    uint8_t *key;
    uint8_t *value;
} Entry;

// Creates the table from memory.
// Must have enough for the table, use `calc_ht_size`.
HashTable *create_ht(uint8_t *memory, uint64_t len, uint32_t key_size,
                     uint32_t val_size);

// Creates a new table from memory, copying in all elements from old_ht.
// Does not change the old_ht.
HashTable *create_from_ht(uint8_t *memory, HashTable *old_ht, uint64_t len);

// Returns 0 on failure, if it is too full.
// Otherwise 1.
uint32_t put_elem_ht(HashTable *ht, uint8_t *key, uint8_t *value);

// Returns 0 if failed to find an element with that key.
// Otherwise returns a reference.
// The reference might become invalid if followed by a delete and put.
// Can be used as `exists` given that non-zero output implies existance.
uint8_t *get_elem_ht(HashTable *ht, uint8_t *key);

// Returns 0 if failed to find an element with that key.
// Otherwise 1.
uint32_t delete_elem_ht(HashTable *ht, uint8_t *key);

// Get the first live entry from the table starting at `idx`.
// Returns pointers to key and value.
// Both pointers will be set to 0 when finished (invalid `idx`).
// `idx` updates to after current entry.
Entry next_elem_ht(HashTable *ht, uint64_t *idx);

// Clears the table for reuse
void clear_ht(HashTable *ht);

#endif // GOT_H_
