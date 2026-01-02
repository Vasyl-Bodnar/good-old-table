/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "got.h"

#define GROUP_SIZE 8

uint64_t power_of_two(uint64_t x) {
    if (x <= 1) {
        return 1;
    }
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x + 1;
}

#define calc_control_size(len) (len + (len % GROUP_SIZE))

#define calc_elems_size(len, key_size, val_size)                               \
    (len * key_size + len * val_size + len + (len % GROUP_SIZE))

// Simple hash to use while we don't have a better one
uint64_t fnv1a_hash(uint8_t *input, uint64_t length) {
    uint64_t init = 12698850840868882907ull;
    for (uint64_t i = 0; i < length; i++) {
        init ^= input[i];
        init *= 1111111111111111111;
    }
    return init;
}

HashTable *create_ht(uint8_t *memory, uint64_t len, uint32_t key_size,
                     uint32_t val_size) {
    HashTable *ht = (HashTable *)memory;
    ht->key_size = key_size;
    ht->val_size = val_size;
    ht->length = 0;
    ht->capacity = power_of_two(len);
    memset(ht->elems, 0, calc_control_size(len));
    return ht;
}

HashTable *create_from_ht(uint8_t *memory, HashTable *old_ht,
                          uint64_t new_len) {
    HashTable *new_ht =
        create_ht(memory, new_len, old_ht->key_size, old_ht->val_size);

    Entry entry;
    uint64_t idx;
    while ((entry = next_elem_ht(old_ht, &idx)).key) {
        put_elem_ht(new_ht, entry.key, entry.value);
    }

    return new_ht;
}

uint32_t elem_size(HashTable *ht) { return ht->key_size + ht->val_size; }

void copy_elem(HashTable *ht, uint64_t idx, uint8_t *key, uint8_t *value) {
    uint8_t *elem = ht->elems + calc_control_size(ht->capacity);
    uint8_t *addr = elem + idx * elem_size(ht);
    memcpy(addr, key, ht->key_size);
    memcpy(addr + ht->key_size, value, ht->val_size);
}

uint32_t put_elem_ht(HashTable *ht, uint8_t *key, uint8_t *value) {
    uint8_t *control = ht->elems;
    if (ht->length == ((ht->capacity * 4) / 5))
        return 0;

    uint64_t keyhash = fnv1a_hash(key, ht->key_size);
    uint64_t hi = keyhash & 0xFE00000000000000ull;
    uint64_t lo = keyhash ^ hi;
    hi >>= 56;

    // TODO: SIMD versions
    for (uint64_t i = lo & (ht->capacity - 1); i < ht->capacity; i++) {
        if (!(control[i] & 1)) {
            control[i] = hi | 1;
            copy_elem(ht, i, key, value);
            return 1;
        }
    }

    return 0;
}

uint8_t *get_elem_ht(HashTable *ht, uint8_t *key) {
    uint8_t *control = ht->elems;
    uint8_t *elem = ht->elems + calc_control_size(ht->capacity);

    uint64_t keyhash = fnv1a_hash(key, ht->key_size);
    uint64_t hi = keyhash & 0xFE00000000000000ull;
    uint64_t lo = keyhash ^ hi;
    hi >>= 56;

    // TODO: SIMD versions
    for (uint64_t i = lo & (ht->capacity - 1); i < ht->capacity; i++) {
        if (!(hi ^ (control[i] ^ 1))) {
            if (!memcmp(key, elem + i * elem_size(ht), ht->key_size)) {
                return elem + i * elem_size(ht) + ht->key_size;
            }
        }
    }

    return 0;
}

uint32_t delete_elem_ht(HashTable *ht, uint8_t *key) {
    uint8_t *control = ht->elems;
    uint8_t *elem = ht->elems + calc_control_size(ht->capacity);

    uint64_t keyhash = fnv1a_hash(key, ht->key_size);
    uint64_t hi = keyhash & 0xFE00000000000000ull;
    uint64_t lo = keyhash ^ hi;
    hi >>= 56;

    // TODO: SIMD versions
    for (uint64_t i = lo & (ht->capacity - 1); i < ht->capacity; i++) {
        if (!(hi ^ (control[i] ^ 1))) {
            if (!memcmp(key, elem + i * elem_size(ht), ht->key_size)) {
                control[i] ^= 1;
                return 1;
            }
        }
    }

    return 0;
}

Entry next_elem_ht(HashTable *ht, uint64_t *idx) {
    if (!idx || *idx >= ht->capacity) {
        return (Entry){0, 0};
    }

    uint8_t *control = ht->elems;
    uint8_t *elem = ht->elems + calc_control_size(ht->capacity);

    // TODO: SIMD versions
    for (uint64_t i = *idx; i < ht->capacity; i++) {
        if (control[i] & 1) {
            *idx = i + 1;
            return (Entry){
                elem + i * elem_size(ht),
                elem + i * elem_size(ht) + ht->key_size,
            };
        }
    }

    *idx = ht->capacity;
    return (Entry){0, 0};
}

void clear_ht(HashTable *ht) {
    ht->length = 0;
    memset(ht->elems, 0, calc_control_size(ht->capacity));
}

// TODO: Malloc+growth wrappers over non-dynamic variants
// Potentially allow providing own alloc function
#ifdef DYNAMIC
typedef struct DynHashTable {
    uint32_t key_size;
    uint32_t val_size;
    uint64_t length;
    uint64_t capacity;
    uint8_t elems[];
} DynHashTable;

DynHashTable *create_dht(uint64_t len, uint32_t key_size, uint32_t val_size) {
    uint8_t *mem = malloc(calc_ht_size(len, key_size, val_size));
    return (DynHashTable *)create_ht(mem, len, key_size, val_size);
}

void delete_dht(DynHashTable *dht) { free(dht); }
#endif
