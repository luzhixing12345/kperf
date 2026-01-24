
#include <elf.h>
#include <gelf.h>
#include <linux/limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libdwarf/libdwarf.h>
#include <libiberty/demangle.h>
#include <dwarf.h>  
#include <libgen.h>
#include "log.h"
#include "parse_elf.h"
#include "utils.h"
#include "symbol.h"

extern enum language_type language;

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

static const char *lang_to_str(Dwarf_Unsigned lang) {
    switch (lang) {
        case DW_LANG_C:
            return "C";
        case DW_LANG_C89:
            return "C89";
        case DW_LANG_C99:
            return "C99";
        case DW_LANG_C11:
            return "C11";
        case DW_LANG_C_plus_plus:
            return "C++";
        case DW_LANG_C_plus_plus_11:
            return "C++11";
        case DW_LANG_C_plus_plus_14:
            return "C++14";
        case DW_LANG_Rust:
            return "Rust";
        default:
            return "Unknown";
    }
}

static enum language_type get_language_type_enum(Dwarf_Unsigned lang) {
    switch (lang) {
        case DW_LANG_C:
        case DW_LANG_C89:
        case DW_LANG_C99:
        case DW_LANG_C11:
            return LANGUAGE_C;
        case DW_LANG_C_plus_plus:
        case DW_LANG_C_plus_plus_11:
        case DW_LANG_C_plus_plus_14:
            return LANGUAGE_CPP;
        case DW_LANG_Rust:
            return LANGUAGE_RUST;
        default:
            return LANGUAGE_UNKNOWN;
    }
}

static Dwarf_Unsigned get_dwarf_language_type(int fd) {
    Dwarf_Debug dbg;
    Dwarf_Error err;

    if (dwarf_init(fd, DW_DLC_READ, NULL, NULL, &dbg, &err) != DW_DLV_OK) {
        ERR("dwarf_init failed\n");
        return -1;
    }

    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Unsigned abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Unsigned next_cu_header;

    while (dwarf_next_cu_header(
               dbg, &cu_header_length, &version_stamp, &abbrev_offset, &address_size, &next_cu_header, &err) ==
           DW_DLV_OK) {
        Dwarf_Die cu_die;

        if (dwarf_siblingof(dbg, NULL, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        Dwarf_Attribute attr;
        if (dwarf_attr(cu_die, DW_AT_language, &attr, &err) == DW_DLV_OK) {
            Dwarf_Unsigned lang;
            if (dwarf_formudata(attr, &lang, &err) == DW_DLV_OK) {
                DEBUG("CU language: %s (%llu)\n", lang_to_str(lang), (unsigned long long)lang);
                return lang;
            }
        } else {
            DEBUG("CU language: <not specified>\n");
        }

        dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
    }

    dwarf_finish(dbg, &err);
    return -1;
}

int has_dwarf_info(Elf *e) {
    /* First check whether ELF contains DWARF sections (.debug_info or .debug_line)
     * using libelf; if not present, return -1 immediately.
     */
    int has_dwarf = 0;
    Elf_Scn *scn_check = NULL;
    GElf_Shdr shdr_check;
    size_t shstrndx;
    if (elf_getshdrstrndx(e, &shstrndx) == 0) {
        while ((scn_check = elf_nextscn(e, scn_check)) != NULL) {
            if (gelf_getshdr(scn_check, &shdr_check) != &shdr_check)
                continue;
            const char *sname = elf_strptr(e, shstrndx, shdr_check.sh_name);
            if (!sname)
                continue;
            if (strcmp(sname, ".debug_info") == 0 || strcmp(sname, ".debug_line") == 0 ||
                strcmp(sname, ".zdebug_info") == 0 || strcmp(sname, ".zdebug_line") == 0) {
                has_dwarf = 1;
                break;
            }
        }
    }
    elf_end(e);
    return has_dwarf;

    // /* Try using libbfd to get file/line info from DWARF if available */
    // bfd_init();
    // bfd *abfd = bfd_openr(elf_path, NULL);
    // if (abfd && bfd_check_format(abfd, bfd_object)) {
    //     long storage = bfd_get_symtab_upper_bound(abfd);
    //     if (storage > 0) {
    //         asymbol **syms = (asymbol **)malloc(storage);
    //         if (syms) {
    //             long symcount = bfd_canonicalize_symtab(abfd, syms);
    //             DEBUG("symcount = %d\n", symcount);
    //             if (symcount > 0) {
    //                 /* iterate function symbols and try to get source file/line */
    //                 for (long i = 0; i < symcount; i++) {
    //                     asymbol *sym = syms[i];
    //                     if (!(sym->flags & BSF_FUNCTION))
    //                         continue;
    //                     asection *sec = sym->section;
    //                     if (!sec)
    //                         continue;
    //                     if (!(sec->flags & SEC_CODE))
    //                         continue;

    //                     const char *name = sym->name ? sym->name : "<noname>";
    //                     bfd_vma addr = sym->value + sec->vma;
    //                     uint64_t runtime_addr = map_start + addr - map_offset;

    //                     const char *filename = NULL;
    //                     const char *funcname = NULL;
    //                     unsigned int line = 0;
    //                     if (bfd_find_nearest_line(abfd, sec, NULL, addr - sec->vma, &filename, &funcname, &line)) {
    //                         // DEBUG("FUNC %-30s RUNTIME=0x%lx FILE=%s:%u\n",
    //                         //       name,
    //                         //       (unsigned long)runtime_addr,
    //                         //       filename ? filename : "??",
    //                         //       line);
    //                     } else {
    //                         // DEBUG("FUNC %-30s RUNTIME=0x%lx\n", name, (unsigned long)runtime_addr);
    //                     }
    //                     add_symbol(st, name, runtime_addr, module_name);
    //                 }
    //             }
    //             free(syms);
    //         }
    //     }
    //     bfd_close(abfd);
    //     return 0;
    // } else {
    //     DEBUG("no dwarf info found\n");
    //     return -1;
    // }
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

int load_elf_symbol(struct symbol_table *st, char *elf_path, uint64_t map_start, uint64_t map_offset,
                    char *module_name) {
    DEBUG("load elf file: %s\n", elf_path);

    if (elf_version(EV_CURRENT) == EV_NONE) {
        fprintf(stderr, "libelf init failed\n");
        return 1;
    }

    int fd = open(elf_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
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

    if (language == -1 && has_dwarf_info(e)) {
        // if no DWARF sections, try to get language type from build ID
        // do not get language type from ELF if DWARF is not present
        Dwarf_Unsigned lang = get_dwarf_language_type(fd);
        if (lang != -1) {
            language = get_language_type_enum(lang);
            DEBUG("set language type to %s\n", lang_to_str(lang));
        }
    }

    /* if libbfd path above didn't work, fall back to debuglink/build-id search
     * and finally to symbol table parsing via libelf (existing logic)
     */
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