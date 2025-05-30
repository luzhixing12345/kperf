
#include "elf.h"

/*
 * load FUNC symbols refering to the section indicated by the offset, relocate
 * the virtual address
 */
void parse_elf64(FILE *fp, unsigned long long v_addr, unsigned long long v_size, unsigned long long v_offset,
                 STORE_T &store) {
    Elf64_Ehdr ehdr;
    int rc = fread(&ehdr, sizeof(ehdr), 1, fp);
    if (rc != 1)
        return;
    int n, s, i;
    unsigned long long offset;

    // load program headers
    unsigned long long p_vaddr, p_size;
    n = ehdr.e_phnum;
    s = ehdr.e_phentsize;
    offset = ehdr.e_phoff;
    Elf64_Phdr phdr;
    for (i = 0; i < n; i++) {
        rc = fseek(fp, offset, SEEK_SET);
        if (rc < 0) {
            perror("fail to seek");
            return;
        }
        rc = fread(&phdr, sizeof(phdr), 1, fp);
        if (rc != 1) {
            perror("fail to read program header");
            return;
        }
        if (phdr.p_flags & PF_X) {
            if (phdr.p_offset == v_offset) {
                p_vaddr = phdr.p_vaddr;
                p_size = phdr.p_memsz;
                if (p_size == 0)
                    p_size = 0xffffffff;
                break;
            }
        }
        offset += s;
    }
    if (i >= n) {
        printf("No program header match offset found, fail to load\n");
        return;
    }

    // load section headers
    n = ehdr.e_shnum;
    s = ehdr.e_shentsize;
    offset = ehdr.e_shoff;
    Elf64_Shdr shdr;
    std::vector<Elf64_Shdr> headers;
    for (i = 0; i < n; i++) {
        rc = fseek(fp, offset, SEEK_SET);
        if (rc < 0) {
            perror("fail to seek");
            return;
        }
        rc = fread(&shdr, sizeof(shdr), 1, fp);
        if (rc != 1) {
            perror("fail to read sec header");
            return;
        }
        headers.push_back(shdr);
        offset += s;
    }
    Elf64_Sym symb;
    unsigned long long faddr, fsize;
    unsigned long long size, item_size;
    int link, ix, flink, k;
    char fname[128];
    for (i = 0; i < n; i++) {
        switch (headers[i].sh_type) {
            case SHT_SYMTAB:
            case SHT_DYNSYM:
                offset = headers[i].sh_offset;
                size = headers[i].sh_size;
                item_size = headers[i].sh_entsize;
                link = headers[i].sh_link;
                if (link <= 0)
                    break;
                for (k = 0; k + item_size <= size; k += item_size) {
                    rc = fseek(fp, offset + k, SEEK_SET);
                    if (rc < 0)
                        continue;
                    rc = fread(&symb, sizeof(symb), 1, fp);
                    if (rc != 1)
                        continue;
                    if (ELF64_ST_TYPE(symb.st_info) != STT_FUNC)
                        continue;
                    flink = symb.st_shndx;
                    if (flink == 0)
                        continue;
                    fsize = symb.st_size;  // if (fsize==0) continue;
                    faddr = symb.st_value;
                    if (faddr > p_vaddr + p_size)
                        continue;
                    ix = symb.st_name;
                    if (ix == 0)
                        continue;
                    rc = fseek(fp, headers[link].sh_offset + ix, SEEK_SET);
                    if (rc < 0)
                        continue;
                    if (fgets(fname, sizeof(fname), fp) == NULL)
                        continue;
                    faddr = faddr - p_vaddr + v_addr;
                    if (store.count(faddr)) {
                        if (store[faddr].second < fsize)
                            store[faddr] = make_pair(std::string(fname), fsize);
                    } else
                        store[faddr] = make_pair(std::string(fname), fsize);
                }
                break;
            default:
                break;
        }
    }
}

int load_symbol_from_file(const char *path, unsigned long long addr, unsigned long long size, unsigned long long offset,
                          STORE_T &store) {
    // printf("loading symble from %s\n", path);
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        perror("fail to open file");
        return -1;
    }
    char ident[EI_NIDENT], c;
    int err = 0;
    int rc = fread(ident, sizeof(ident), 1, fp);
    if (rc != 1) {
        perror("fail to read ident");
        err = -1;
        goto end;
    }
    if (ident[0] != 0x7f) {
        printf("not a elf file\n");
        err = -1;
        goto end;
    }
    c = ident[4];
    rc = fseek(fp, 0, SEEK_SET);
    if (rc < 0) {
        perror("fail to rewind");
        goto end;
    }
    if (c == ELFCLASS32) {
        printf("32bit elf not supported yet\n");
        err = -2;
        goto end;
    } else if (c == ELFCLASS64) {
        parse_elf64(fp, addr, size, offset, store);
    }

end:
    fclose(fp);
    return err;
}

static unsigned long long parse_hex(char *p, int *n) {
    unsigned long long r = 0;
    int i = 0;
    *n = 0;
    while (p[i] == ' ' || p[i] == '\t') i++;
    if (p[i] == 0)
        return 0;
    if (p[i + 1] == 'x')
        i += 2;
    int v;
    while (p[i]) {
        if (p[i] >= '0' && p[i] <= '9')
            v = p[i] - '0';
        else if (p[i] >= 'a' && p[i] <= 'f')
            v = 10 + p[i] - 'a';
        else if (p[i] >= 'A' && p[i] <= 'F')
            v = 10 + p[i] - 'A';
        else
            break;
        r = (r << 4) + v;
        i++;
    }
    *n = i;
    return r;
}

STORE_T *load_symbol_pid(int pid) {
    
    char path[256];
    sprintf(path, "/proc/%d/maps", pid);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return NULL;

    STORE_T *store = new STORE_T();
    unsigned long long start, end, offset, inode;
    char *p;
    int i, c, j;
    char fname[128], xx[64], xxx[32], mod[16], idx[16];
    while (1) {
        p = fgets(path, sizeof(path), fp);
        if (p == NULL)
            break;
        if (sscanf(p, "%s %s %s %s %llu %s", xx, mod, xxx, idx, &inode, fname) != 6)
            continue;
        i = 0;
        c = 0;
        start = parse_hex(xx, &c);
        if (c == 0)
            continue;
        i += c;
        if (p[i] != '-')
            continue;
        i++;
        end = parse_hex(xx + i, &c);
        if (c == 0)
            continue;
        // parse type
        for (j = 0; j < 8; j++)
            if (mod[j] == 'x')
                break;
        if (j >= 8)
            continue;
        if (fname[0] != '/')
            continue;
        offset = parse_hex(xxx, &c);
        if (c == 0)
            continue;
        // remaining should contains '/' indicating this mmap is refering to a file
        sprintf(path, "/proc/%d/root%s", pid, fname);
        load_symbol_from_file(path, start, end - start, offset, *store);
    }
    fclose(fp);
    if (store->size() == 0) {
        delete store;
        store = NULL;
    }
    return store;
}

/* parse kernel func symbols from /proc/kallsyms */
K_STORE_T *load_kernel() {
    FILE *fp = fopen("/proc/kallsyms", "r");
    if (fp == NULL)
        return NULL;
    char *p;
    unsigned long long addr;
    int c;
    K_STORE_T *store = new K_STORE_T();
    char bb[128], adr[128], type[8], name[128];
    while (1) {
        p = fgets(bb, sizeof(bb), fp);
        if (p == NULL)
            break;
        if (sscanf(p, "%s %s %s", adr, type, name) != 3)
            continue;
        ;
        if (type[0] != 't' && type[0] != 'T')
            continue;
        addr = parse_hex(adr, &c);
        if (c == 0)
            continue;
        (*store)[addr] = std::string(name);
    }
    return store;
    fclose(fp);
}
