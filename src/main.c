#include "lib/got.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GROUP_SIZE 8

#define calc_control_size(len) (len + (len % GROUP_SIZE))

#define calc_elems_size(len, key_size, val_size)                               \
    (len * 8 + len * key_size + len * val_size + len + (len % GROUP_SIZE))

#define calc_ht_size(len, key_size, val_size)                                  \
    (sizeof(HashTable) + len * 8 + len * key_size + len * val_size + len +     \
     (len % GROUP_SIZE))

// Simple hash to use while we don't have a better one
uint64_t fnv1a_hash(uint8_t *input, uint64_t length) {
    uint64_t init = 12698850840868882907ull;
    for (uint64_t i = 0; i < length; i++) {
        init ^= input[i];
        init *= 1111111111111111111;
    }
    return init;
}

#define hash fnv1a_hash

typedef struct HashTable {
    uint32_t key_size;
    uint32_t val_size;
    uint64_t length;
    uint64_t capacity;
    uint8_t elems[]; // Note that this also includes control bytes
} HashTable;

HashTable *create_ht(uint8_t *memory, uint64_t len, uint32_t key_size,
                     uint32_t val_size) {
    HashTable *ht = (HashTable *)memory;
    ht->key_size = key_size;
    ht->val_size = val_size;
    ht->length = 0;
    ht->capacity = len;
    memset(ht->elems, 0, calc_control_size(len));
    return ht;
}

inline uint32_t elem_size(HashTable *ht) {
    return 8 + ht->key_size + ht->val_size;
}

inline void copy_elem(HashTable *ht, uint64_t idx, uint64_t keyhash,
                      uint8_t *key, uint8_t *value) {
    uint8_t *elem = ht->elems + calc_control_size(ht->capacity);
    uint8_t *addr = elem + idx * elem_size(ht);
    memcpy(addr, &keyhash, sizeof(keyhash));
    memcpy(addr + sizeof(keyhash), key, ht->key_size);
    memcpy(addr + sizeof(keyhash) + ht->key_size, value, ht->val_size);
}

// Multiple variants for specialization with an integer are provided

// In case if it is too full it returns 0
// Otherwise 1
uint64_t put_elem_ht(HashTable *ht, uint8_t *key, uint8_t *value) {
    uint8_t *control = ht->elems;
    if (ht->length == ((ht->capacity * 4) / 5))
        return 0;

    uint64_t keyhash = hash(key, ht->key_size);
    uint64_t lo = keyhash & 0x7F;
    uint64_t hi = keyhash ^ lo;

    for (uint64_t i = 0; i < ht->capacity; i++) {
        if (!(control[i] & 1)) {
            control[i] = (lo << 1) | 1;
            copy_elem(ht, i, hi, key, value);
            return 1;
        }
    }

    return 0;
}
uint64_t put_elemk_ht(HashTable *ht, uint64_t key, uint8_t *value);
uint64_t put_elemv_ht(HashTable *ht, uint8_t *key, uint64_t value);
uint64_t put_elemkv_ht(HashTable *ht, uint64_t key, uint64_t value);

// This is a reference that might become invalid if followed by a delete
// Returns 0 on failure
uint8_t *get_elem_ht(HashTable *ht, uint8_t *key) {
    uint8_t *control = ht->elems;
    uint8_t *elem = ht->elems + calc_control_size(ht->capacity);

    uint64_t keyhash = hash(key, ht->key_size);
    uint64_t lo = keyhash & 0x7F;
    uint64_t hi = keyhash ^ lo;

    for (uint64_t i = 0; i < ht->capacity; i++) {
        if (!((lo << 1) ^ (control[i] ^ 1))) {
            if (((uint64_t *)(elem + i * elem_size(ht)))[0] == hi) {
                return elem + i * elem_size(ht) + sizeof(hi) + ht->key_size;
            }
        }
    }

    return 0;
}
uint8_t *get_elemk_ht(HashTable *ht, uint64_t key);
uint64_t get_elemv_ht(HashTable *ht, uint8_t *key);
uint64_t get_elemkv_ht(HashTable *ht, uint64_t key);

void delete_elem_ht(HashTable *ht, uint8_t *key);
void delete_elemk_ht(HashTable *ht, uint64_t key);
void delete_elemv_ht(HashTable *ht, uint8_t *key);
void delete_elemkv_ht(HashTable *ht, uint64_t key);

void clear_ht(HashTable *ht) {
    ht->length = 0;
    memset(ht->elems, 0, calc_control_size(ht->capacity));
}

// TODO: Malloc+growth wrappers over non-dynamic variants
// Potentially allow providing own alloc function
#ifdef DYNAMIC
#endif
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

int main(void) {
    uint8_t mem[calc_ht_size(10, 4, 4)];
    HashTable *ht = create_ht(mem, 10, 4, 4);
    printf("%lu\n", put_elem_ht(ht, (uint8_t *)"123", (uint8_t *)"321"));
    printf("%lu\n", put_elem_ht(ht, (uint8_t *)"124", (uint8_t *)"421"));
    printf("%lu\n", put_elem_ht(ht, (uint8_t *)"125", (uint8_t *)"521"));
    printf("%lu\n", put_elem_ht(ht, (uint8_t *)"126", (uint8_t *)"621"));
    printf("%lu\n", put_elem_ht(ht, (uint8_t *)"127", (uint8_t *)"721"));

    printf("%s\n", get_elem_ht(ht, (uint8_t *)"123"));
    printf("%s\n", get_elem_ht(ht, (uint8_t *)"124"));
    printf("%s\n", get_elem_ht(ht, (uint8_t *)"125"));
    printf("%s\n", get_elem_ht(ht, (uint8_t *)"126"));
    printf("%s\n", get_elem_ht(ht, (uint8_t *)"127"));
    return 0;
}
