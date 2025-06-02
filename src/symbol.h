
#pragma once

#include <stdint.h>
#include <cstddef>
#include <cstdint>
#include "common.h"

#define SYMBOL_TABLE_INIT_SIZE (1024)
#define SYMBOL_TABLE_GROWTH_FACTOR (2)
#define SYMBOL_TABLE_KERNEL_INIT_SIZE (100000)

struct Symbol {
    char *name;
    uint64_t addr;
    char *module_name;

    // private
    long count;
};

struct SymbolTable {
    Symbol *symbols;
    size_t size;
    size_t capacity;
};

int load_kernel_symbol();
int load_user_symbol(char *path);