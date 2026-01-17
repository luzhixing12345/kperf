
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "log.h"
#include "symbol.h"
#include "utils.h"
#include "parse_elf.h"

int load_user_symbols(struct symbol_table *st, int pid) {
    char maps_path[256];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    FILE *fp = fopen(maps_path, "r");
    if (!fp) {
        perror("fopen maps");
        return 0;
    }

    char line[512];
    uint64_t start, end, offset, inode;
    char fname[128], mod[16], dev[16];
    while (fgets(line, sizeof(line), fp)) {
        /* load execute section symbols */
        // printf("line: %s", line);
        if (strstr(line, "r-xp")) {
            // start    end      perm offset   dev   inode  pathname
            // 00400000-0040b000 r-xp 00000000 fd:01 123456 /usr/bin/ls
            int n = sscanf(line, "%lx-%lx %s %lx %s %lu %s", &start, &end, mod, &offset, dev, &inode, fname);
            printf("%s\n", fname);
            if (n != 7) {
                continue;
            }
            // only load elf file, ignore [stack] [vsdo] [vsyscall]...
            if (fname[0] != '/') {
                continue;
            }
            load_elf_file(st, fname, start, offset);
        }
    }

    fclose(fp);

    DEBUG("load user symbol %d items\n", st->size);
    return 0;
}

// https://blog.csdn.net/qq_42931917/article/details/129943916
int load_kernel_symbols(struct symbol_table *st) {
    FILE *fp = fopen("/proc/kallsyms", "r");
    if (fp == NULL) {
        perror("fail to open /proc/kallsyms");
        return -1;
    }
    char *p;
    unsigned long long addr;
    int c;
    int is_module = 0;
    int n;
    char bb[128], adr[128], type[8], name[128], module[128];
    while (1) {
        p = fgets(bb, sizeof(bb), fp);
        if (p == NULL)
            break;
        n = sscanf(p, "%s %s %s\t[%s]", adr, type, name, module);
        if (n != 3 && n != 4) {
            fprintf(stderr, "fail to parse line: %s", p);
            continue;
        }
        is_module = (n == 4);

        // we only care about T(global symbols) and t(local symbols)
        if (type[0] != 't' && type[0] != 'T')
            continue;
        addr = parse_hex(adr, &c);
        if (c == 0)
            continue;
        add_symbol(st, name, addr, is_module ? module : NULL);
    }
    fclose(fp);

    INFO("load kernel symbol %d items\n", st->size);
    return 0;
}

void init_symbol_table(struct symbol_table *st) {
    memset(st, 0, sizeof(*st));
    st->size = 0;
    st->capacity = SYMBOL_TABLE_KERNEL_INIT_SIZE;
    st->symbols = malloc(sizeof(struct symbol) * st->capacity);
    if (st->symbols == NULL) {
        perror("fail to malloc");
        exit(1);
    }
}

void free_symbol_table(struct symbol_table *st) {
    if (!st) {
        return;
    }
    for (int i = 0; i < st->size; i++) {
        free(st->symbols[i].name);
        if (st->symbols[i].module != NULL) {
            free(st->symbols[i].module);
        }
    }
    free(st->symbols);
}

void add_symbol(struct symbol_table *st, const char *name, uint64_t addr, const char *module) {
    if (st->size == st->capacity) {
        st->capacity *= SYMBOL_TABLE_GROWTH_FACTOR;
        st->symbols = realloc(st->symbols, sizeof(struct symbol) * st->capacity);
        if (st->symbols == NULL) {
            perror("fail to realloc");
            exit(1);
        }
    }
    st->symbols[st->size].addr = addr;
    st->symbols[st->size].name = strdup(name);
    st->symbols[st->size].module = module ? strdup(module) : NULL;
    st->size++;
}