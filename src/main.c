
#include <stdio.h>
#include "kernel.h"

int main(int argc, char** argv) {
    struct SymbolTable* symbol_table = load_kernel_symbols();
    if (symbol_table == NULL) {
        fprintf(stderr, "fail to load kernel symbols\n");
        return -1;
    }
    print_kernel_symbols(symbol_table);
    free_kernel_symbols(symbol_table);
    return 0;
}
