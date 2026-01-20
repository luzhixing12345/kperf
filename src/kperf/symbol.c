
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
#include <elf.h>
#include <gelf.h>
#include "log.h"
#include "symbol.h"
#include "utils.h"
#include "parse_elf.h"

extern int enable_debug;


// static uint64_t find_dt_debug_address(pid_t pid, char *exe_path) {
//     /* Use libelf to parse the ELF file on disk and locate the dynamic segment
//      * and the index of the DT_DEBUG entry. Then read the DT_DEBUG pointer
//      * value from the target process memory at the corresponding dynamic entry
//      * address using a single ptrace peek. This avoids iterating dynamic
//      * entries in target memory with read_target_mem.
//      */
//     if (elf_version(EV_CURRENT) == EV_NONE) {
//         WARNING("libelf initialization failed\n");
//         return 0;
//     }

//     int fd = open(exe_path, O_RDONLY);
//     if (fd < 0) {
//         WARNING("open(%s) failed: %s\n", exe_path, strerror(errno));
//         return 0;
//     }

//     Elf *e = elf_begin(fd, ELF_C_READ, NULL);
//     if (!e) {
//         close(fd);
//         WARNING("elf_begin failed\n");
//         return 0;
//     }
//     GElf_Ehdr eh;
//     if (gelf_getehdr(e, &eh) == NULL) {
//         elf_end(e);
//         close(fd);
//         WARNING("gelf_getehdr failed\n");
//         return 0;
//     }

//     size_t phnum = 0;
//     if (elf_getphdrnum(e, &phnum) != 0)
//         phnum = eh.e_phnum;

//     uint64_t dyn_vaddr = 0, dyn_memsz = 0, dyn_offset = 0;
//     for (size_t i = 0; i < phnum; i++) {
//         GElf_Phdr ph;
//         if (gelf_getphdr(e, i, &ph) == NULL)
//             break;
//         if (ph.p_type == PT_DYNAMIC) {
//             dyn_vaddr = ph.p_vaddr;
//             dyn_memsz = ph.p_memsz;
//             dyn_offset = ph.p_offset;
//             break;
//         }
//     }

//     if (!dyn_vaddr || !dyn_memsz) {
//         elf_end(e);
//         close(fd);
//         WARNING("no PT_DYNAMIC found in %s\n", exe_path);
//         return 0;
//     }

//     /* Read dynamic entries from file to find the DT_DEBUG index */
//     uint64_t entry_count = dyn_memsz / sizeof(Elf64_Dyn);
//     int debug_index = -1;
//     for (uint64_t i = 0; i < entry_count; i++) {
//         Elf64_Dyn d;
//         off_t off = (off_t)(dyn_offset + i * sizeof(Elf64_Dyn));
//         ssize_t r = pread(fd, &d, sizeof(d), off);
//         if (r != sizeof(d))
//             break;
//         if ((int64_t)d.d_tag == DT_DEBUG) {
//             debug_index = (int)i;
//             break;
//         }
//         if ((int64_t)d.d_tag == DT_NULL)
//             break;
//     }

//     elf_end(e);
//     if (debug_index < 0) {
//         close(fd);
//         WARNING("DT_DEBUG not found in file for %s\n", exe_path);
//         return 0;
//     }

//     /* find mapping that contains the PT_DYNAMIC file offset
//      * We must match the maps entry whose file offset range covers dyn_offset.
//      * The mapping line gives a start address and the file offset that is mapped
//      * to that start; the memory address for dyn_offset is therefore:
//      *    mapping_start + (dyn_offset - mapping_file_offset)
//      */
//     char maps_path[256];
//     snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
//     FILE *fp = fopen(maps_path, "r");
//     if (!fp) {
//         close(fd);
//         return 0;
//     }
//     char line[512];
//     uint64_t dyn_addr_mem = 0;
//     while (fgets(line, sizeof(line), fp)) {
//         if (!strstr(line, exe_path))
//             continue;
//         uint64_t start = 0, end = 0, map_off = 0;
//         char perms[16], dev[16], inode_str[32], path[256];
//         int n = sscanf(line, "%lx-%lx %15s %lx %15s %31s %255s", &start, &end, perms, &map_off, dev, inode_str,
//         path); if (n < 4)
//             continue;
//         uint64_t mapsize = end - start;
//         if (dyn_offset >= map_off && dyn_offset < map_off + mapsize) {
//             dyn_addr_mem = start + (dyn_offset - map_off);
//             break;
//         }
//     }
//     fclose(fp);
//     if (!dyn_addr_mem) {
//         close(fd);
//         WARNING("failed to locate dynamic section in process maps for %s\n", exe_path);
//         return 0;
//     }
//     uint64_t entry_addr = dyn_addr_mem + (uint64_t)debug_index * sizeof(Elf64_Dyn);
//     DEBUG("DT_DEBUG entry address = 0x%lx\n", entry_addr);

//     /* read the DT_DEBUG entry from target memory using ptrace peek (single word reads)
//      * d_tag (8 bytes) + d_un (8 bytes). We'll read two words and reconstruct the entry.
//      */
//     Elf64_Dyn dval;
//     long v0 = ptrace_peek(pid, entry_addr);
//     if (errno) {
//         close(fd);
//         return 0;
//     }
//     long v1 = ptrace_peek(pid, entry_addr + sizeof(long));
//     if (errno) {
//         close(fd);
//         return 0;
//     }
//     memcpy(&dval, &v0, sizeof(long));
//     memcpy(((uint8_t *)&dval) + sizeof(long), &v1, sizeof(long));

//     if ((int64_t)dval.d_tag == DT_DEBUG) {
//         uint64_t ret = (uint64_t)dval.d_un.d_ptr;
//         DEBUG("found DT_DEBUG pointer = 0x%lx\n", ret);
//         close(fd);
//         return ret;
//     }

//     close(fd);
//     WARNING("tag: %ld, d_un: %ld\n", dval.d_tag, dval.d_un.d_ptr);
//     WARNING("DT_DEBUG entry mismatched after ptrace read\n");
//     return 0;
// }

static int symbol_cmp(const void *a, const void *b) {
    const struct symbol *sa = a, *sb = b;
    if (sa->addr < sb->addr)
        return -1;
    else if (sa->addr > sb->addr)
        return 1;
    else
        return 0;
}

int load_user_symbols(struct symbol_table *st, int pid) {
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
            load_elf_symbol(st, fname, start, offset);
        }
    }
    fclose(fp);
    DEBUG("load user symbol %d items\n", st->size);
    if (st->size == 0) {
        ERR("fail to load user symbols, something went wrong!\n");
        return -1;
    }
    qsort(st->symbols, st->size, sizeof(struct symbol), symbol_cmp);
    if (enable_debug) {
        save_symbol_table(st, "ust.txt");
        DEBUG("save user symbol table to ust.txt\n");
    }
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
        DEBUG("realloc symbol table to %d\n", st->capacity);
    }
    st->symbols[st->size].addr = addr;
    st->symbols[st->size].name = strdup(name);
    st->symbols[st->size].module = module ? strdup(module) : NULL;
    st->size++;
}

void save_symbol_table(struct symbol_table *st, const char *file) {
    FILE *fp = fopen(file, "w");
    if (fp == NULL) {
        perror("fail to open file");
        return;
    }
    for (int i = 0; i < st->size; i++) {
        fprintf(fp, "%s\t0x%lx\t%s\n", st->symbols[i].name, st->symbols[i].addr, st->symbols[i].module);
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