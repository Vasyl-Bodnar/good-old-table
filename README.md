# good-old-table
A fine hash table in C.

Supports get, put, delete on elements, as well as iteration. 
Base version allows you to provide your own allocated memory for creation and resizing.
Dynamic version uses malloc wrappers and handles memory for you.

## Usage
For the base variant that does not manage memory on its own:
```c
uint8_t mem[calc_ht_size(10, 4, 4)];
HashTable *ht = create_ht(mem, 10, 4, 4); // Create from memory, does not care about source

uint64_t ret = put_elem_ht(ht, (uint8_t *)"123", (uint8_t *)"321"); // Put elements
if (!ret) puts("Too full!");

uint8_t *value = get_elem_ht(ht, (uint8_t *)"123"); // Get elements
if (!value) puts("Did not find any!");
else printf("Got a string! %s\n", value);

ret = delete_elem_ht(ht, (uint8_t *)"123"); // Delete elements
if (!ret) puts("Did not find any!");

uint8_t new_mem[calc_ht_size(20, 4, 4)];
HashTable *new_ht = create_from_ht(new_mem, ht, 20); // Note that key and value sizes are the same
puts("New table!");

value = get_elem_ht(new_ht, (uint8_t *)"123"); // Get what you expect
if (!value) puts("Did not find any!");
else printf("Got a string! %s\n", value);
value = get_elem_ht(new_ht, (uint8_t *)"124");
if (!value) puts("Did not find any!");
else printf("Got a string! %s\n", value);

Entry entry; // Can iterate quite easily using this pattern
uint64_t idx = 0;
while ((entry = next_elem_ht(new_ht, &idx)).key) {
    printf("Entry at %zu: key: %s value: %s\n", idx, entry.key, entry.value);
}

clear_ht(ht); // Now old one is empty, and can be reused
```

For dynamic version, simply use `_dht` variants, note that put will require a double pointer to the table in order to replace it.

## Build
Utilizes [build-scm](https://github.com/Vasyl-Bodnar/build-scm). 
You need to have Guile scheme installed, which should be available from your package manager if not installed already.

Simply run `./build.scm` or `guile build.scm` at project root. 
The libraries are in the generated `build` folder

You can also just copy `got.c` and `got.h` for your own uses.

## License
The library is licensed under MPL version 2.0.

Code examples in `README` and `main.c` are under BSD-3.
