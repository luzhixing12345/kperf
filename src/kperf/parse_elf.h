
#pragma once

#include "symbol.h"

int load_elf_file(struct symbol_table *st, char *elf_path, uint64_t map_start, uint64_t map_offset);