
#pragma once

#include <stdint.h>

#define SYMBOL_TABLE_INIT_SIZE        (1024)
#define SYMBOL_TABLE_GROWTH_FACTOR    (2)
#define SYMBOL_TABLE_KERNEL_INIT_SIZE (150000)

struct symbol {
    char *name;
    uint64_t addr;
    char *module;

    // private
    long called_count;
};

struct symbol_table {
    struct symbol *symbols;
    int size;
    int capacity;
    int module_count;
};

void init_symbol_table(struct symbol_table *st);
void free_symbol_table(struct symbol_table *st);
void add_symbol(struct symbol_table *st, const char *name, uint64_t addr, const char *module);
int load_user_symbols(struct symbol_table *st, int pid);
int load_kernel_symbols(struct symbol_table *st);
uint64_t get_symbol_addr(struct symbol_table *st, const char *name);
struct symbol *find_symbol(struct symbol_table *st, uint64_t addr);