
#pragma once

#include <stdio.h>
#include <elf.h>

#define MAX_FUNCTION_NAME 64

// kernel symbol
struct Symbol {
    char symbol_name[MAX_FUNCTION_NAME];
    unsigned long addr;
    char type;
};

typedef struct {
    int count;
    struct Symbol *symbols;
} SymbolTable;

typedef struct ELF {
    void *addr;
    Elf64_Ehdr ehdr;   // ELF头
    Elf64_Shdr *shdr;  // 段表
    Elf64_Off shstrtab_offset;
} ELF;

/**
 * @brief parse kernel func symbols from /proc/kallsyms
 *
 * @return struct KsymbolTable*
 */
SymbolTable *load_kernel_symbols();

void print_symbol_table(SymbolTable *symbols);

/**
 * @brief free kernel symbol table
 *
 * @param symbols
 */
void free_symbol_table(SymbolTable *symbols);

/**
 * @brief parse user func symbols from elf
 *
 * @param elf_path
 * @return SymbolTable*
 */
SymbolTable *load_user_symbols(const char *elf_path);
