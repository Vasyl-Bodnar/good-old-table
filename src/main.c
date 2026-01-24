// SPDX-License-Identifier: BSD-3-Clause
#include "lib/got.h"
#include <stdio.h>

int main(void) {
    uint8_t mem[calc_ht_size(10, 4, 4)];
    HashTable *ht = create_ht(
        mem, 10, 4, 4); // Create from memory, does not care about source

    uint32_t ret = put_elem_ht(ht, "123", "321"); // Put elements
    if (!ret)
        puts("Too full!"); // If too full, nothing changes in the table
    ret = put_elem_ht(ht, "124", "421");
    if (!ret)
        puts("Too full!");
    ret = put_elem_ht(ht, "125", "521");
    if (!ret)
        puts("Too full!");

    char *value = get_elem_ht(ht, "123"); // Get elements
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);
    value = get_elem_ht(ht, "124");
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);
    value = get_elem_ht(ht, "125");
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);

    ret = delete_elem_ht(ht, "123"); // Delete elements
    if (!ret)
        puts("Did not find any to delete!");

    value = get_elem_ht(ht, "123");
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);

    // We can create a new table from the old one to "resize"
    // The live elems carry over and are rehashed
    // Note that key and value sizes are the same
    uint8_t new_mem[calc_ht_size(20, 4, 4)];
    HashTable *new_ht = create_from_ht(new_mem, ht, 20);
    puts("New table!");

    value = get_elem_ht(new_ht, "123"); // Get what you expect
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);
    value = get_elem_ht(new_ht, "124");
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);
    value = get_elem_ht(new_ht, "125");
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);

    Entry entry; // Can iterate quite easily using this pattern
    size_t idx = 0;
    while ((entry = next_elem_ht(new_ht, &idx)).key) {
        printf("Entry at %zu: key: %s value: %s\n", idx, (char *)entry.key,
               (char *)entry.value);
    }

    clear_ht(ht); // Now, first table is empty, and can even be reused
    puts("Cleared the old table!");

    idx = 0; // Will find nothing in the old table
    while ((entry = next_elem_ht(ht, &idx)).key) {
        printf("Entry at %zu: key: %s value: %s\n", idx, (char *)entry.key,
               (char *)entry.value);
    }

#ifdef DYNAMIC_TABLE
    // Dynamic, much the same, except for all the malloc
    HashTable *dht = create_dht(2, 4, 4);
    ret = put_elem_dht(&dht, "124", "421");
    if (!ret)
        puts("Too full!");
    ret = put_elem_dht(&dht, "125", "521");
    if (!ret)
        puts("Too full!");
    // Can replace! Returns 2 instead of 1 in that case
    ret = put_elem_dht(&dht, "125", "621");
    if (!ret)
        puts("Too full!");

    value = get_elem_dht(dht, "123");
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);
    value = get_elem_dht(dht, "125");
    if (!value)
        puts("Did not find any!");
    else
        printf("Got a string! %s\n", value);

    idx = 0;
    while ((entry = next_elem_dht(dht, &idx)).key) {
        printf("Entry at %zu: key: %s value: %s\n", idx, (char *)entry.key,
               (char *)entry.value);
    }
#endif // DYNAMIC_TABLE
    return 0;
}
