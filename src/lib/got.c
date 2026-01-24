/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "got.h"

#if !(defined(memcpy) && defined(memcmp))
#include <string.h>
#endif

#ifndef hash
#define hash fnv1a_hash
#endif

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
    ((len + (len % GROUP_SIZE)) * (key_size + 1) +                             \
     (len + (len % GROUP_SIZE)) * val_size)

uint64_t fnv1a_hash(const uint8_t *input, const size_t length) {
    uint64_t init = 12698850840868882907ull;
    for (size_t i = 0; i < length; i++) {
        init ^= input[i];
        init *= 1111111111111111111;
    }
    return init;
}

HashTable *create_ht(void *memory, const size_t len, const size_t key_size,
                     const size_t val_size) {
    HashTable *ht = memory;
    ht->key_size = key_size;
    ht->val_size = val_size;
    ht->length = 0;
    ht->capacity = power_of_two(len);
    memset(ht->elems, 0x80, calc_control_size(ht->capacity));
    return ht;
}

HashTable *create_from_ht(void *memory, HashTable *old_ht,
                          const size_t new_len) {
    HashTable *new_ht =
        create_ht(memory, new_len, old_ht->key_size, old_ht->val_size);

    Entry entry;
    size_t idx = 0;
    while ((entry = next_elem_ht(old_ht, &idx)).key) {
        put_elem_ht(new_ht, entry.key, entry.value);
    }

    return new_ht;
}

uint32_t elem_size(const HashTable *ht) { return ht->key_size + ht->val_size; }

void copy_elem(HashTable *ht, const size_t idx, const void *key,
               const void *value) {
    uint8_t *elem = ht->elems + calc_control_size(ht->capacity);
    uint8_t *addr = elem + idx * elem_size(ht);
    memcpy(addr, key, ht->key_size);
    memcpy(addr + ht->key_size, value, ht->val_size);
}

// TODO: Consider some method of wrapping as an optimization?
// TODO: Our loads are unaligned
uint32_t put_elem_ht(HashTable *ht, const void *key, const void *value) {
    if (ht->length >= ((ht->capacity * 4) / 5))
        return 0;

    uint8_t *control = ht->elems;
    uint8_t *elem = ht->elems + calc_control_size(ht->capacity);

    uint64_t keyhash = fnv1a_hash(key, ht->key_size);
    uint64_t hi = keyhash & 0xFE00000000000000ull;
    uint64_t lo = keyhash ^ hi;
    hi >>= 57;

#ifdef __SSE2__
    const __m128i emptyv = _mm_set1_epi8(0x80);
    __m128i hiv = _mm_set1_epi8(hi);
    for (size_t i = lo & (ht->capacity - 1); i < ht->capacity;
         i += GROUP_SIZE) {
        __m128i controlv = _mm_loadu_si128((__m128i *)(control + i));
        int res = _mm_movemask_epi8(_mm_cmpeq_epi8(hiv, controlv));
        while (res) {
            size_t j = i + __builtin_ctz(res);
            if (!memcmp(key, elem + j * elem_size(ht), ht->key_size)) {
                memcpy(elem + j * elem_size(ht) + ht->key_size, value,
                       ht->val_size);
                return 2;
            }
            res &= res - 1;
        }

        res = _mm_movemask_epi8(_mm_and_si128(controlv, emptyv));
        if (res) {
            size_t j = i + __builtin_ctz(res);
            control[j] = hi;
            copy_elem(ht, j, key, value);
            ht->length += 1;
            return 1;
        }
    }

#else // SWAR
    const uint64_t emptyv = 0x8080808080808080ull;
    const uint64_t onev = 0x0101010101010101ull;
    uint64_t hiv = onev * hi;
    for (size_t i = lo & (ht->capacity - 1); i < ht->capacity;
         i += GROUP_SIZE) {
        uint64_t controlv = *(uint64_t *)(control + i);
        uint64_t res = (((hiv ^ controlv) - onev) & ~(hiv ^ controlv)) &
                       0x8080808080808080ull;
        while (res) {
            size_t j = i + (__builtin_ctzll(res) >> 3);
            if (!memcmp(key, elem + j * elem_size(ht), ht->key_size)) {
                memcpy(elem + j * elem_size(ht) + ht->key_size, value,
                       ht->val_size);
                return 2;
            }
            res &= res - 1;
        }

        res = controlv & emptyv;
        if (res) {
            size_t j = i + __builtin_ctzll(res) >> 3;
            control[j] = hi;
            copy_elem(ht, j, key, value);
            ht->length += 1;
            return 1;
        }
    }

#endif

    return 0;
}

void *get_elem_ht(HashTable *ht, const void *key) {
    uint8_t *control = ht->elems;
    uint8_t *elem = ht->elems + calc_control_size(ht->capacity);

    uint64_t keyhash = fnv1a_hash(key, ht->key_size);
    uint64_t hi = keyhash & 0xFE00000000000000ull;
    uint64_t lo = keyhash ^ hi;
    hi >>= 57;

#ifdef __SSE2__
    const __m128i emptyv = _mm_set1_epi8(0x80);
    __m128i hiv = _mm_set1_epi8(hi);
    for (size_t i = lo & (ht->capacity - 1); i < ht->capacity;
         i += GROUP_SIZE) {
        __m128i controlv = _mm_loadu_si128((__m128i *)(control + i));
        int res = _mm_movemask_epi8(_mm_cmpeq_epi8(hiv, controlv));
        while (res) {
            size_t j = i + __builtin_ctz(res);
            if (!memcmp(key, elem + j * elem_size(ht), ht->key_size)) {
                return elem + j * elem_size(ht) + ht->key_size;
            }
            res &= res - 1;
        }

        if (_mm_movemask_epi8(_mm_cmpeq_epi8(controlv, emptyv))) {
            return 0;
        }
    }

#else // SWAR
    const uint64_t emptyv = 0x8080808080808080ull;
    const uint64_t onev = 0x0101010101010101ull;
    uint64_t hiv = onev * hi;
    for (size_t i = lo & (ht->capacity - 1); i < ht->capacity;
         i += GROUP_SIZE) {
        uint64_t controlv = *(uint64_t *)(control + i);
        uint64_t res = (((hiv ^ controlv) - onev) & ~(hiv ^ controlv)) &
                       0x8080808080808080ull;
        while (res) {
            size_t j = i + (__builtin_ctzll(res) >> 3);
            if (!memcmp(key, elem + j * elem_size(ht), ht->key_size)) {
                return elem + j * elem_size(ht) + ht->key_size;
            }
            res &= res - 1;
        }

        if ((((controlv ^ emptyv) - onev) & ~(controlv ^ emptyv))) {
            return 0;
        }
    }

#endif

    return 0;
}

uint32_t delete_elem_ht(HashTable *ht, const void *key) {
    uint8_t *control = ht->elems;
    uint8_t *elem = ht->elems + calc_control_size(ht->capacity);

    uint64_t keyhash = fnv1a_hash(key, ht->key_size);
    uint64_t hi = keyhash & 0xFE00000000000000ull;
    uint64_t lo = keyhash ^ hi;
    hi >>= 57;

#ifdef __SSE2__
    const __m128i emptyv = _mm_set1_epi8(0x80);
    __m128i hiv = _mm_set1_epi8(hi);
    for (size_t i = lo & (ht->capacity - 1); i < ht->capacity;
         i += GROUP_SIZE) {
        __m128i controlv = _mm_loadu_si128((__m128i *)(control + i));
        int res = _mm_movemask_epi8(_mm_cmpeq_epi8(hiv, controlv));
        while (res) {
            size_t j = i + __builtin_ctz(res);
            if (!memcmp(key, elem + j * elem_size(ht), ht->key_size)) {
                control[j] = 0xFE;
                ht->length -= 1;
                return 1;
            }
            res &= res - 1;
        }

        if (_mm_movemask_epi8(_mm_cmpeq_epi8(controlv, emptyv))) {
            return 0;
        }
    }

#else // SWAR
    const uint64_t emptyv = 0x8080808080808080ull;
    const uint64_t onev = 0x0101010101010101ull;
    uint64_t hiv = onev * hi;
    for (size_t i = lo & (ht->capacity - 1); i < ht->capacity;
         i += GROUP_SIZE) {
        uint64_t controlv = *(uint64_t *)(control + i);
        uint64_t res = (((hiv ^ controlv) - onev) & ~(hiv ^ controlv)) &
                       0x8080808080808080ull;
        while (res) {
            size_t j = i + (__builtin_ctzll(res) >> 3);
            if (!memcmp(key, elem + j * elem_size(ht), ht->key_size)) {
                control[j] = 0xFE;
                ht->length -= 1;
                return 1;
            }
            res &= res - 1;
        }

        if ((((controlv ^ emptyv) - onev) & ~(controlv ^ emptyv))) {
            return 0;
        }
    }

#endif

    return 0;
}

Entry next_elem_ht(HashTable *ht, size_t *idx) {
    if (!idx || *idx >= ht->capacity) {
        return (Entry){0, 0};
    }

    uint8_t *control = ht->elems;
    void *elem = ht->elems + calc_control_size(ht->capacity);

    // TODO: SIMD versions
    for (size_t i = *idx; i < ht->capacity; i++) {
        if (!(control[i] & 0x80)) {
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
    memset(ht->elems, 0x80, calc_control_size(ht->capacity));
}

#ifdef DYNAMIC_TABLE

#if !(defined(alloc) && defined(dealloc))
#include <stdlib.h>
#define alloc malloc
#define dealloc free
#endif

HashTable *create_dht(const size_t len, const size_t key_size,
                      const size_t val_size) {
    void *mem = alloc(calc_ht_size(len, key_size, val_size));
    return create_ht(mem, len, key_size, val_size);
}

HashTable *realloc_dht(HashTable *old_dht, const size_t new_len) {
    HashTable *new_dht =
        create_dht(new_len, old_dht->key_size, old_dht->val_size);

    Entry entry;
    size_t idx = 0;
    while ((entry = next_elem_ht(old_dht, &idx)).key) {
        put_elem_ht(new_dht, entry.key, entry.value);
    }

    free(old_dht);

    return new_dht;
}

uint32_t put_elem_dht(HashTable **dht, const void *key, const void *value) {
    uint32_t ret = put_elem_ht(*dht, key, value);
    if (!ret) {
        *dht = realloc_dht(*dht, (*dht)->capacity << 1);
        return put_elem_dht(dht, key, value);
    }
    return ret;
}

void *get_elem_dht(HashTable *dht, const void *key) {
    return get_elem_ht(dht, key);
}

uint32_t delete_elem_dht(HashTable *dht, const void *key) {
    return delete_elem_ht(dht, key);
}

Entry next_elem_dht(HashTable *dht, size_t *idx) {
    return next_elem_ht(dht, idx);
}

void clear_dht(HashTable *dht) {
    dht->length = 0;
    memset(dht->elems, 0, calc_control_size(dht->capacity));
}

void delete_dht(HashTable *dht) { dealloc(dht); }
#endif // DYNAMIC_TABLE
