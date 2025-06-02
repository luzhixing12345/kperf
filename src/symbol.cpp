
#include "symbol.h"
#include <cstdlib>
#include <stdio.h>
#include <clib/clib.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/utsname.h>

struct SymbolTable *kernel_symbol;  // global kernel symbol table
struct SymbolTable *user_symbol;    // global user symbol table

static FILE *open_kernel_symbol_file() {
    FILE *fp = fopen("/proc/kallsyms", "r");
    if (!fp) {
        if (errno == EACCES) {
            perror("fail to open /proc/kallsyms");
            DEBUG("Permission denied to open /proc/kallsyms\n");
            fprintf(stderr, "please use sudo to run kperf\n");
            return NULL;
        } else if (errno == ENOENT) {
            perror("fail to open /proc/kallsyms");
            DEBUG("No such file or directory: /proc/kallsyms\n");
            fprintf(stderr, "use CONFIG_KALLSYMS=y to enable /proc/kallsyms\n");
        } else {
            perror("fail to open /proc/kallsyms");
            return NULL;
        }
    } else {
        return fp;
    }

    // fail to open /proc/kallsyms, try to open /boot/System.map-$(uname -r)
    struct utsname buffer;

    if (uname(&buffer) != 0) {
        perror("uname");
        return NULL;
    }
    char path[128];
    sprintf(path, "/boot/System.map-%s", buffer.release);
    fp = fopen(path, "r");
    if (!fp) {
        perror("fail to open /boot/System.map");
        return NULL;
    }

    return fp;
}

FILE *open_user_symbol_file(const char *path) {
    
    return fopen(path, "r");
}

// 动态扩展符号表容量
static int expand_symbol_table(struct SymbolTable *table) {
    size_t new_capacity = table->capacity * SYMBOL_TABLE_GROWTH_FACTOR;
    struct Symbol *new_symbols = (struct Symbol *)realloc(table->symbols, sizeof(struct Symbol) * new_capacity);
    if (!new_symbols) {
        fprintf(stderr, "Failed to expand symbol table\n");
        return -1;
    }

    table->symbols = new_symbols;
    table->capacity = new_capacity;
    return 0;
}

// 添加符号到符号表
static int add_symbol(struct SymbolTable *table, uint64_t addr, const char *name, const char *module_name) {
    // 检查是否需要扩容
    if (table->size >= table->capacity) {
        if (expand_symbol_table(table) != 0) {
            return -1;
        }
    }

    struct Symbol *sym = &table->symbols[table->size];

    // 分配并复制符号名
    sym->name = strdup(name);
    if (!sym->name) {
        fprintf(stderr, "Failed to allocate memory for symbol name\n");
        return -1;
    }

    // 设置地址
    sym->addr = addr;

    // 分配并复制模块名（如果有）
    if (module_name && strlen(module_name) > 0) {
        sym->module_name = strdup(module_name);
        if (!sym->module_name) {
            free(sym->name);
            fprintf(stderr, "Failed to allocate memory for module name\n");
            return -1;
        }
    } else {
        sym->module_name = NULL;
    }

    // 初始化计数
    sym->count = 0;

    table->size++;
    return 0;
}

int load_kernel_symbol() {
    FILE *fp = open_kernel_symbol_file();
    if (!fp) {
        return -1;
    }
    kernel_symbol = (struct SymbolTable *)malloc(sizeof(struct SymbolTable));
    kernel_symbol->symbols = (struct Symbol *)malloc(sizeof(struct Symbol) * SYMBOL_TABLE_KERNEL_INIT_SIZE);
    memset(kernel_symbol->symbols, 0, sizeof(struct Symbol) * SYMBOL_TABLE_KERNEL_INIT_SIZE);
    kernel_symbol->size = 0;
    kernel_symbol->capacity = SYMBOL_TABLE_KERNEL_INIT_SIZE;

    // /proc/kallsyms
    // 000000000000c000 A exception_stacks
    // 0000000000018000 A entry_stack_storage
    // ffffffffc0a0c730 t bpf_prog_6deef7357e7b4530	[bpf]
    // ffffffffc0a0e954 t bpf_prog_6deef7357e7b4530	[bpf]
    char line[512];
    char addr_str[33], type, name[256], module_name[256];
    uint64_t addr;
    while (fgets(line, sizeof(line), fp)) {
        // 解析基本字段：地址 类型 符号名 [模块名]
        memset(module_name, 0, sizeof(module_name)); // 清空模块名
        int fields = sscanf(line, "%s %c %s\t[%s]", addr_str, &type, name, module_name);
        if (fields < 3) {
            ERR("Invalid line format: %s", line);
            continue;
        }

        // 只处理函数符号（t = 本地函数, T = 全局函数）
        if (type != 't' && type != 'T') {
            continue;
        }

        // 转换地址
        char *endptr;
        addr = strtoull(addr_str, &endptr, 16);
        if (*endptr != '\0') {
            fprintf(stderr, "Warning: Invalid address format: %s\n", addr_str);
            continue;
        }

        // 添加符号到表中
        if (add_symbol(kernel_symbol, addr, name, module_name) == 0) {
            // DEBUG("Added symbol: %s at %lx\n", name, addr);
        } else {
            fprintf(stderr, "Failed to add symbol: %s\n", name);
        }
    }
    kprintf("loaded %lu kernel function symbol entries\n", kernel_symbol->size);
    fclose(fp);
    return 0;
}

int load_user_symbol(char *path) {
    FILE *fp = open_user_symbol_file(path);
    if (!fp) {
        return -1;
    }

    user_symbol = (struct SymbolTable *)malloc(sizeof(struct SymbolTable));
    user_symbol->symbols = (struct Symbol *)malloc(sizeof(struct Symbol) * SYMBOL_TABLE_INIT_SIZE);
    memset(user_symbol->symbols, 0, sizeof(struct Symbol) * SYMBOL_TABLE_INIT_SIZE);
    user_symbol->size = 0;
    user_symbol->capacity = SYMBOL_TABLE_INIT_SIZE;

    exit(0);
    return 0;
}