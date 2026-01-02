# good-old-table
A fine hash table in C.

A work in progress

## Usage
For the base variant that does not manage memory on its own:
```c
uint8_t mem[calc_ht_size(10, 4, 4)];
HashTable *ht = create_ht(mem, 10, 4, 4); // Create from memory, does not care about source

uint64_t ret = put_elem_ht(ht, (uint8_t *)"123", (uint8_t *)"321"); // Put elements
if (!ret) puts("Too full!");

uint8_t *value = get_elem_ht(ht, (uint8_t *)"123"); // Get elements
if (!value) puts("Did not find any!");
printf("Got a string! %s\n", value);

ret = delete_elem_ht(ht, (uint8_t *)"123"); // Delete elements
if (!ret) puts("Did not find any!");

clear_ht(ht); // Now, it is empty, and can be reused
```

## Build
Utilizes [build-scm](https://github.com/Vasyl-Bodnar/build-scm). 
You need to have Guile scheme installed, which should be available from your package manager if not installed already.

Simply run `./build.scm` or `guile build.scm` at project root. 
The libraries are in the generated `build` folder

You can also just copy `got.c` and `got.h` for your own uses.

## License
The library is licensed under MPL version 2.0.

Code examples in `README` and `main.c` are under BSD-3.
