
#pragma once

#include <stdio.h>

#define MAX_FUNCTION_NAME 64

// kernel symbol
struct Symbol {
    char symbol_name[MAX_FUNCTION_NAME];
    unsigned long addr;
    char type;
};

struct SymbolTable {
    int count;
    struct Symbol *symbols;
};

/**
 * @brief parse kernel func symbols from /proc/kallsyms
 *
 * @return struct KsymbolTable*
 */
struct SymbolTable *load_kernel_symbols();

void print_kernel_symbols(struct SymbolTable *symbols);

/**
 * @brief free kernel symbol table
 *
 * @param symbols
 */
void free_kernel_symbols(struct SymbolTable *symbols);