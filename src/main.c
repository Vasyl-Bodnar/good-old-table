// SPDX-License-Identifier: BSD-3-Clause
#include "lib/got.h"
#include <stdio.h>

int main(void) {
    uint8_t mem[calc_ht_size(10, 4, 4)];
    HashTable *ht = create_ht(
        mem, 10, 4, 4); // Create from memory, does not care about source

    uint64_t ret =
        put_elem_ht(ht, (uint8_t *)"123", (uint8_t *)"321"); // Put elements
    if (!ret)
        puts("Too full!");

    uint8_t *value = get_elem_ht(ht, (uint8_t *)"123"); // Get elements
    if (!value)
        puts("Did not find any!");
    printf("Got a string! %s\n", value);

    ret = delete_elem_ht(ht, (uint8_t *)"123"); // Delete elements
    if (!ret)
        puts("Did not find any!");

    clear_ht(ht); // Now, it is empty, and can be reused
    return 0;
}
