
#include "kernel.h"
#include <stdio.h>
#include <stdlib.h>

struct SymbolTable *load_kernel_symbols() {
    FILE *fp = fopen("/proc/kallsyms", "r");
    if (fp == NULL) {
        perror("fail to open /proc/kallsyms");
        return NULL;
    }
    char *p;
    char buffer[128];
    struct SymbolTable *symbol_table = (struct SymbolTable *)malloc(sizeof(struct SymbolTable));
    symbol_table->count = 0;
    while (1) {
        p = fgets(buffer, sizeof(buffer), fp);
        if (p == NULL) {
            break;
        }
        symbol_table->count++;
    }
    symbol_table->symbols = (struct Symbol *)malloc(sizeof(struct Symbol) * symbol_table->count);
    int i = 0;
    while (1) {
        p = fgets(buffer, sizeof(buffer), fp);
        if (p == NULL) {
            break;
        }
        struct Symbol *symbol = &symbol_table->symbols[i];
        if (sscanf(p, "%lu %c %s", &symbol->addr, &symbol->type, symbol->symbol_name) != 3) {
            fprintf(stderr, "sscanf error\n");
            continue;
        }
        if (symbol->type != 't' && symbol->type != 'T' && symbol->type != 'W') {
            fprintf(stderr, "type error %c\n", symbol->type);
            continue;
        }
        i++;
    }
    fclose(fp);
    return symbol_table;
}

void print_kernel_symbols(struct SymbolTable *symbol_table) {
    for (int i = 0; i < symbol_table->count; i++) {
        struct Symbol *symbol = &symbol_table->symbols[i];
        printf("[0x%lx] [%c] [%s]\n", symbol->addr, symbol->type, symbol->symbol_name);
    }
    printf("count: %d\n", symbol_table->count);
}

void free_kernel_symbols(struct SymbolTable *symbol_table) {
    free(symbol_table->symbols);
    free(symbol_table);
}