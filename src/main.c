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
    ret = put_elem_ht(ht, (uint8_t *)"124", (uint8_t *)"421");
    if (!ret)
        puts("Too full!");
    ret = put_elem_ht(ht, (uint8_t *)"125", (uint8_t *)"521");
    if (!ret)
        puts("Too full!");

    uint8_t *value = get_elem_ht(ht, (uint8_t *)"123"); // Get elements
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);
    value = get_elem_ht(ht, (uint8_t *)"124");
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);
    value = get_elem_ht(ht, (uint8_t *)"125");
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);

    ret = delete_elem_ht(ht, (uint8_t *)"123"); // Delete elements
    if (!ret)
        puts("Did not find any!");
    value = get_elem_ht(ht, (uint8_t *)"123");
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);

    // We can create a new table from the old one to "resize"
    // The live elems carry over and are rehashed
    uint8_t new_mem[calc_ht_size(20, 4, 4)];
    HashTable *new_ht = create_from_ht(
        new_mem, ht, 20); // Note that key and value sizes are the same
    puts("New table!");

    value = get_elem_ht(new_ht, (uint8_t *)"123"); // Get what you expect
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);
    value = get_elem_ht(new_ht, (uint8_t *)"124");
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);

    clear_ht(ht); // Now, first table is empty, and can even be reused
    return 0;
}
