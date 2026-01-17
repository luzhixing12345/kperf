
#define _GNU_SOURCE
#include <elf.h>
#include <gelf.h>
#include <libelf.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "symbol.h"
#include "log.h"

int load_elf_file(struct symbol_table *st, char *elf_path, uint64_t map_start, uint64_t map_offset) {

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

            /* real runtime address = loaded addr(/proc/pid/maps) + elf symbol section bias */
            uint64_t runtime_addr = map_start + sym.st_value - map_offset;
            add_symbol(st, name, runtime_addr, NULL);

            // printf("FUNC %-30s ELF=0x%lx RUNTIME=0x%lx\n", name, (unsigned long)sym.st_value, runtime_addr);
        }
    }

    elf_end(e);
    close(fd);
    return 0;
}
