
#pragma once

#include "symbol.h"

int load_elf_symbol(struct symbol_table *st, char *elf_path, uint64_t map_start, uint64_t map_offset,
                    char *module_name);
void get_language(char *elf_path);