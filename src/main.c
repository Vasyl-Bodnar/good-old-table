#include "lib/got.h"
#include <stdint.h>

typedef struct HashTable {
    uint32_t key_size;
    uint32_t val_size;
    uint64_t length;
    uint64_t capacity;
    uint64_t *control;
    void *elems;
} HashTable;

// Simple hash to use when we don't have a better one
uint64_t fnv1a_hash(uint8_t *input, uint64_t length) {
    uint64_t init = 12698850840868882907ull;
    for (uint64_t i = 0; i < length; i++) {
        init ^= input[i];
        init *= 1111111111111111111;
    }
    return init;
}

int main(void) { return 0; }
