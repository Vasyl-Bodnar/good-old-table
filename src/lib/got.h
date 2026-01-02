/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef LIB_H_
#define LIB_H_

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

// Creates the table from memory.
// Must have enough for the table, use `calc_ht_size`.
HashTable *create_ht(uint8_t *memory, uint64_t len, uint32_t key_size,
                     uint32_t val_size);

// Returns 0 on failure, if it is too full.
// Otherwise 1.
uint32_t put_elem_ht(HashTable *ht, uint8_t *key, uint8_t *value);

// Returns 0 if failed to find an element with that key.
// Otherwise returns a reference.
// The reference might become invalid if followed by a delete and put.
uint8_t *get_elem_ht(HashTable *ht, uint8_t *key);

// Returns 0 if failed to find an element with that key.
// Otherwise 1.
uint32_t delete_elem_ht(HashTable *ht, uint8_t *key);

// Clears the table for reuse
void clear_ht(HashTable *ht);

#endif // LIB_H_
