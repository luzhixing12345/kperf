
#include <assert.h>
#include <elf.h>
#include <gelf.h>
#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <elfutils/libdw.h>
#include <elfutils/libdwfl.h>
#include <dwarf.h>
#include <libgen.h>
#include "log.h"
#include "parse_elf.h"
#include "utils.h"
#include "symbol.h"

/* ------------------ read .gnu_debuglink ------------------ */
int read_gnu_debuglink(Elf *elf, char *name, size_t name_sz, uint32_t *crc) {
    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;

    while ((scn = elf_nextscn(elf, scn))) {
        gelf_getshdr(scn, &shdr);
        size_t shstrndx;
        elf_getshdrstrndx(elf, &shstrndx);

        const char *sname = elf_strptr(elf, shstrndx, shdr.sh_name);
        if (!sname)
            continue;

        if (strcmp(sname, ".gnu_debuglink") == 0) {
            Elf_Data *data = elf_getdata(scn, NULL);
            if (!data || data->d_size < 4)
                return -1;

            strncpy(name, data->d_buf, name_sz - 1);
            name[name_sz - 1] = 0;

            size_t off = strlen(name) + 1;
            off = (off + 3) & ~3; /* align 4 */

            if (off + 4 <= data->d_size)
                memcpy(crc, (char *)data->d_buf + off, 4);
            else
                *crc = 0;

            return 0;
        }
    }
    return -1;
}

/* ------------------ read .note.gnu.build-id ------------------ */
int read_build_id(Elf *elf, unsigned char *id, size_t *id_len) {
    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;

    while ((scn = elf_nextscn(elf, scn))) {
        gelf_getshdr(scn, &shdr);
        size_t shstrndx;
        elf_getshdrstrndx(elf, &shstrndx);

        const char *sname = elf_strptr(elf, shstrndx, shdr.sh_name);
        if (!sname || strcmp(sname, ".note.gnu.build-id"))
            continue;

        Elf_Data *data = elf_getdata(scn, NULL);
        if (!data)
            return -1;

        size_t off = 0;
        while (off < data->d_size) {
            Elf64_Nhdr *nh = (Elf64_Nhdr *)((char *)data->d_buf + off);
            off += sizeof(*nh);

            const char *name = (char *)data->d_buf + off;
            off += (nh->n_namesz + 3) & ~3;

            const unsigned char *desc = (unsigned char *)data->d_buf + off;
            off += (nh->n_descsz + 3) & ~3;

            if (nh->n_type == NT_GNU_BUILD_ID && strcmp(name, "GNU") == 0) {
                memcpy(id, desc, nh->n_descsz);
                *id_len = nh->n_descsz;
                return 0;
            }
        }
    }
    return -1;
}

int has_dwarf_info(int fd) {
    Dwarf *dbg = NULL;
    dbg = dwarf_begin(fd, DWARF_C_READ);
    if (!dbg) {
        DEBUG("no DWARF info\n");
        return 0;
    }
    dwarf_end(dbg);
    return 1;
}

int has_debug_link(Elf *e, char *path) {
    /* if libbfd path above didn't work, fall back to debuglink/build-id search
     * and finally to symbol table parsing via libelf (existing logic)
     */
    char dbgname[256] = {0};
    uint32_t crc = 0;
    unsigned char build_id[64];
    size_t build_id_len = 0;

    // readelf --debug-dump=links <elf>
    int has_debuglink = read_gnu_debuglink(e, dbgname, sizeof(dbgname), &crc) == 0;
    // readelf --notes <elf>
    int has_buildid = read_build_id(e, build_id, &build_id_len) == 0;

    if (has_debuglink && has_buildid) {
        /*
         * https://sourceware.org/gdb/current/onlinedocs/gdb.html/Separate-Debug-Files.html
         *
         * find debug info from paths like:
         * - /usr/lib/debug/.build-id/ab/cdef1234.debug
         * - /usr/bin/ls.debug
         * - /usr/bin/.debug/ls.debug
         * - /usr/lib/debug/usr/bin/ls.debug.
         *
         * However gdb uses:
         * (gdb) show debug-file-directory
         * The directory where separate debug symbols are searched for is "/usr/lib/debug".
         *
         * so we only use the gdb default path for now
         */
        char hex[128];
        for (size_t i = 1; i < build_id_len; i++) sprintf(hex + (i - 1) * 2, "%02x", build_id[i]);
        snprintf(path, PATH_MAX, "/usr/lib/debug/.build-id/%02x/%s.debug", build_id[0], hex);
        if (file_exists(path)) {
            DEBUG("found debug link file %s\n", path);
            return 0;
        } else {
            DEBUG("debug link file %s not found\n", path);
            return -1;
        }
    } else {
        DEBUG("no debuglink or buildid found\n");
        return -1;
    }
}

static int get_dwarf_function_info(Dwarf_Die *cu_die, Dwarf_Die *die, uint64_t *addr, const char **filename,
                                   int *lineno) {
    Dwarf_Addr low_pc, high_pc;

    if (dwarf_lowpc(die, &low_pc) == 0 && dwarf_highpc(die, &high_pc) == 0) {
        *addr = low_pc;
        Dwarf_Files *files;
        size_t nfiles;
        if (dwarf_getsrcfiles(cu_die, &files, &nfiles) == 0) {
            Dwarf_Line *line = dwarf_getsrc_die(cu_die, *addr);
            if (line) {
                *filename = dwarf_linesrc(line, NULL, NULL);
                if (dwarf_lineno(line, lineno) == 0) {
                    return 0;
                }
                ERR("  No line number available\n");
                return 0;
            } else {
                ERR("  No line info available\n");
                return -1;
            }
        } else {
            ERR("  No source files information\n");
            return -1;
        }
    }
    return -1;
}

static int dwarf_process_cu(Dwarf_Die *cu_die, struct symbol_table *st, uint64_t map_bias, char *module_name) {
    Dwarf_Die child_die;
    int res = dwarf_child(cu_die, &child_die);

    while (res == 0) {
        if (dwarf_tag(&child_die) == DW_TAG_subprogram) {
            const char *name = dwarf_diename(&child_die);
            const char *filename = NULL;
            int lineno = -1;
            uint64_t addr = 0;
            if (name) {
                if (get_dwarf_function_info(cu_die, &child_die, &addr, &filename, &lineno) == 0) {
                    uint64_t runtime_addr = map_bias + addr;
                    struct symbol *sym;
                    sym = add_symbol(st, name, runtime_addr, module_name);
                    assert(filename != NULL);
                    assert(lineno != -1);
                    sym->filename = strdup(filename);
                    sym->lineno = lineno;
                }
            } else {
                ERR("No function info available\n");
            }
        }

        Dwarf_Die sibling_die;
        if (dwarf_siblingof(&child_die, &sibling_die) == 0) {
            child_die = sibling_die;
        } else {
            break;
        }
    }

    return DWARF_CB_OK;
}

int load_dwarf(struct symbol_table *st, int fd, uint64_t map_bias, char *module_name) {
    Dwarf *dbg = NULL;
    dbg = dwarf_begin(fd, DWARF_C_READ);
    if (!dbg) {
        ERR("Failed to initialize DWARF debugging: %s\n", dwarf_errmsg(-1));
        close(fd);
        return -1;
    }

    Dwarf_Off cu_offset = 0;
    Dwarf_Off next_cu_offset = 0;
    size_t cu_header_size = 0;
    Dwarf_Die cu_die;

    while (dwarf_nextcu(dbg, cu_offset, &next_cu_offset, &cu_header_size, NULL, NULL, NULL) == 0) {
        Dwarf_Die *cu_die_ptr = dwarf_offdie(dbg, cu_offset + cu_header_size, &cu_die);
        if (cu_die_ptr && dwarf_tag(cu_die_ptr) == DW_TAG_compile_unit) {
            dwarf_process_cu(cu_die_ptr, st, map_bias, module_name);
        }
        cu_offset = next_cu_offset;
    }

    dwarf_end(dbg);
    return 0;
}

int load_elf_symbol(struct symbol_table *st, char *elf_path, uint64_t map_start, uint64_t map_offset,
                    char *module_name) {
    if (elf_version(EV_CURRENT) == EV_NONE) {
        fprintf(stderr, "libelf init failed\n");
        return 1;
    }

    int fd = open(elf_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (has_dwarf_info(fd)) {
        DEBUG("load dwarf file: %s\n", elf_path);
        if (load_dwarf(st, fd, map_start - map_offset, module_name) == 0) {
            DEBUG("finish loading dwarf file: %s\n", elf_path);
        } else {
            ERR("load dwarf failed, use elf symbol table\n");
        }
    }

    Elf *e = elf_begin(fd, ELF_C_READ, NULL);
    if (!e) {
        fprintf(stderr, "elf_begin failed\n");
        return 1;
    }

    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;

    DEBUG("load symbol table from elf file: %s\n", elf_path);
    /* Fallback: read symbol table from the ELF itself (no DWARF) */
    while ((scn = elf_nextscn(e, scn)) != NULL) {
        gelf_getshdr(scn, &shdr);

        if (shdr.sh_type != SHT_SYMTAB && shdr.sh_type != SHT_DYNSYM)
            continue;

        Elf_Data *data = elf_getdata(scn, NULL);
        int count = shdr.sh_size / shdr.sh_entsize;

        for (int i = 0; i < count; i++) {
            GElf_Sym sym;
            gelf_getsym(data, i, &sym);

            if (GELF_ST_TYPE(sym.st_info) != STT_FUNC)
                continue;

            if (sym.st_value == 0)
                continue;

            const char *name = elf_strptr(e, shdr.sh_link, sym.st_name);
            if (!name || !name[0])
                continue;

            uint64_t runtime_addr = map_start + sym.st_value - map_offset;
            add_symbol(st, name, runtime_addr, module_name);
        }
    }

    char debug_path[PATH_MAX];
    if (has_debug_link(e, debug_path) == 0) {
        if (load_elf_symbol(st, debug_path, map_start, map_offset, module_name) == 0) {
            DEBUG("finish loading elf file: %s\n", debug_path);
        }
    }

    elf_end(e);
    close(fd);
    return 0;
}