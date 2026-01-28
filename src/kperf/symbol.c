
#define _GNU_SOURCE
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <elf.h>
#include <link.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <libdwarf/libdwarf.h>
#include <dwarf.h>
#include <elf.h>
#include <gelf.h>
#include <libgen.h>
#include "log.h"
#include "symbol.h"
#include "utils.h"
#include "parse_elf.h"

extern int enable_debug;

static int symbol_cmp(const void *a, const void *b) {
    const struct symbol *sa = a, *sb = b;
    if (sa->addr < sb->addr)
        return -1;
    else if (sa->addr > sb->addr)
        return 1;
    else
        return 0;
}

static void deduplicate_symbols(struct symbol_table *st) {
    if (st->size <= 1)
        return;

    int write_idx = 1;
    int orig_size = st->size;
    for (int i = 1; i < st->size; i++) {
        if (st->symbols[i].addr != st->symbols[write_idx - 1].addr) {
            st->symbols[write_idx] = st->symbols[i];
            write_idx++;
        } else {
            free(st->symbols[i].name);
            if (st->symbols[i].module)
                free(st->symbols[i].module);
            if (st->symbols[i].filename)
                free(st->symbols[i].filename);
        }
    }
    st->size = write_idx;
    DEBUG("deduplicate symbols: %d -> %d items\n", orig_size, st->size);
}

int load_user_symbols(struct symbol_table *st, int pid) {
    if (st->size != 0) {
        WARNING("already get user symbols\n");
        return 0;
    }
    /* First pass: load existing r-xp mappings (executable and any already-loaded libs) */
    char maps_path[256];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    FILE *fp = fopen(maps_path, "r");
    if (!fp) {
        perror("fopen maps");
        return 0;
    }

    /* find exe path */
    char exe_link[64], exe_path[PATH_MAX];
    snprintf(exe_link, sizeof(exe_link), "/proc/%d/exe", pid);
    ssize_t len = readlink(exe_link, exe_path, sizeof(exe_path) - 1);
    if (len <= 0) {
        DEBUG("no exe path\n");
        return -1;
    }
    exe_path[len] = '\0';

    /*
     * read the mapped elf file symbols and add them to the symbol table
     */
    char line[512];
    uint64_t start, end, offset, inode;
    char fname[PATH_MAX], mod[16], dev[16];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "r-xp")) {
            /*
             * The maps file has the following format:
             * start    end      perm offset   dev   inode  fname
             * 00400000-0040b000 r-xp 00000000 fd:01 123456 /usr/bin/ls
             *
             * https://blog.csdn.net/qq_42931917/article/details/129943916
             */
            int n = sscanf(line, "%lx-%lx %s %lx %s %lu %s", &start, &end, mod, &offset, dev, &inode, fname);
            if (n < 7)
                continue;
            /* skip not elf file, like [stack] [vsdo] */
            if (fname[0] != '/')
                continue;
            load_elf_symbol(st, fname, start, offset, basename(fname));
        }
    }
    fclose(fp);
    if (st->size == 0) {
        ERR("fail to load user symbols, something went wrong!\n");
        return -1;
    }
    qsort(st->symbols, st->size, sizeof(struct symbol), symbol_cmp);
    deduplicate_symbols(st);
    if (enable_debug) {
        save_symbol_table(st, "ust.txt");
        DEBUG("save user symbol table to ust.txt\n");
    }
    INFO("load user symbol %d items\n", st->size);
    return 0;

    /*
     * when the dynamic linker loads a shared library, it will add a new entry to the link_map
     * and call _dl_debug_state to notify the debugger.
     *
     * so, we should set breakpoint at _dl_debug_state,
     * and wait for the dynamic linker to hit the breakpoint
     *
     * https://zhuanlan.zhihu.com/p/555962031
     * https://zhuanlan.zhihu.com/p/836833506
     */
    // uint64_t dll_breakpoint_addr = get_symbol_addr(st, "_dl_debug_state");
    // DEBUG("dll_breakpoint_addr = 0x%lx\n", dll_breakpoint_addr);
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
        n = sscanf(p, "%s %s %s\t%s", adr, type, name, module);
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
        // set module's last char to '\0'
        add_symbol(st, name, addr, is_module ? module : "[kernel]");
    }
    fclose(fp);
    if (enable_debug) {
        save_symbol_table(st, "kst.txt");
        DEBUG("save kernel symbol table to kst.txt\n");
    }

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
        if (st->symbols[i].module) {
            free(st->symbols[i].module);
        }
        if (st->symbols[i].filename) {
            free(st->symbols[i].filename);
        }
    }
    free(st->symbols);
    free(st);
}

struct symbol *add_symbol(struct symbol_table *st, const char *name, uint64_t addr, const char *module) {
    if (st->size == st->capacity) {
        st->capacity *= SYMBOL_TABLE_GROWTH_FACTOR;
        st->symbols = realloc(st->symbols, sizeof(struct symbol) * st->capacity);
        if (st->symbols == NULL) {
            perror("fail to realloc");
            exit(1);
        }
        DEBUG("realloc symbol table to %d\n", st->capacity);
    }
    struct symbol *s = &st->symbols[st->size];
    s->addr = addr;
    s->name = strdup(name);
    s->module = module ? strdup(module) : NULL;
    s->filename = NULL;
    s->lineno = 0;
    st->size++;
    return s;
}

void save_symbol_table(struct symbol_table *st, const char *file) {
    FILE *fp = fopen(file, "w");
    if (fp == NULL) {
        perror("fail to open file");
        return;
    }
    for (int i = 0; i < st->size; i++) {
        fprintf(fp, "%35s\t0x%lx\t%s\n", st->symbols[i].name, st->symbols[i].addr, st->symbols[i].module);
    }
    fclose(fp);
}

uint64_t get_symbol_addr(struct symbol_table *st, const char *name) {
    for (int i = 0; i < st->size; i++) {
        if (strcmp(st->symbols[i].name, name) == 0) {
            return st->symbols[i].addr;
        }
    }
    ERR("fail to find symbol %s\n", name);
    return 0;
}