
#include "parse_symbol.h"
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

SymbolTable *load_kernel_symbols() {
    FILE *fp = fopen("/proc/kallsyms", "r");
    if (fp == NULL) {
        perror("fail to open /proc/kallsyms");
        return NULL;
    }
    char *p;
    char buffer[128];
    SymbolTable *symbol_table = (SymbolTable *)malloc(sizeof(SymbolTable));
    symbol_table->count = 0;
    while (1) {
        p = fgets(buffer, sizeof(buffer), fp);
        if (p == NULL) {
            break;
        }
        symbol_table->count++;
    }
    symbol_table->symbols = (struct Symbol *)malloc(sizeof(struct Symbol) * symbol_table->count);
    int i = 0;
    fseek(fp, 0, SEEK_SET);
    while (1) {
        p = fgets(buffer, sizeof(buffer), fp);
        if (p == NULL) {
            break;
        }
        struct Symbol *symbol = &symbol_table->symbols[i];
        if (sscanf(p, "%lx %c %s", &symbol->addr, &symbol->type, symbol->symbol_name) != 3) {
            fprintf(stderr, "sscanf error\n");
            continue;
        }
        if (symbol->type != 't' && symbol->type != 'T' && symbol->type != 'W') {
            fprintf(stderr, "type error %c\n", symbol->type);
            continue;
        }
        i++;
    }
    fclose(fp);
    return symbol_table;
}

void print_symbol_table(SymbolTable *symbol_table) {
    for (int i = 0; i < symbol_table->count; i++) {
        struct Symbol *symbol = &symbol_table->symbols[i];
        printf("[0x%lx]: [%s]\n", symbol->addr, symbol->symbol_name);
    }
    printf("count: %d\n", symbol_table->count);
}

void free_symbol_table(SymbolTable *symbol_table) {
    free(symbol_table->symbols);
    free(symbol_table);
}

/**
 * @brief readelf -s 查看符号表信息
 *
 * @param ELF_file_data
 * @return int
 */
static int load_elf_symbol_table(ELF *ELF_file_data, SymbolTable *symbol_table, unsigned long long start_addr,
                                 unsigned long long offset) {
    // typedef struct {
    //     uint32_t      st_name;
    //     unsigned char st_info;
    //     unsigned char st_other;
    //     uint16_t      st_shndx;
    //     Elf64_Addr    st_value;
    //     uint64_t      st_size;
    // } Elf64_Sym;
    int index = symbol_table->count;

    int section_number = ELF_file_data->ehdr.e_shnum;
    Elf64_Sym *symtab_addr;  // 符号表指针
    int symtab_number;       // 符号表表项的个数
    for (int i = 0; i < section_number; i++) {
        Elf64_Shdr *shdr = &ELF_file_data->shdr[i];
        // SHT_SYMTAB 和 SHT_DYNSYM 类型的段是符号表
        if ((shdr->sh_type == SHT_SYMTAB) || (shdr->sh_type == SHT_DYNSYM)) {
            // sh_link 指向符号表对应的字符串表
            Elf64_Shdr *strtab = &ELF_file_data->shdr[shdr->sh_link];

            // 定位到当前段的起始地址
            symtab_addr = (Elf64_Sym *)((char *)ELF_file_data->addr + shdr->sh_offset);
            // 通过 sh_size 和 Elf64_Sym 结构体大小计算表项数量
            symtab_number = shdr->sh_size / sizeof(Elf64_Sym);
            symbol_table->count += symtab_number;
            symbol_table->symbols =
                (struct Symbol *)realloc(symbol_table->symbols, symbol_table->count * sizeof(struct Symbol));
            for (int j = 0; j < symtab_number; j++) {
                // 对于每一个表项 symtab_addr[j] => Elf64_Sym
                // st_info 的低4位用于符号类型 0-3      => ELF64_ST_TYPE
                // st_info 的高4位用于符号绑定信息 4-7  => ELF64_ST_BIND
                if (ELF64_ST_TYPE(symtab_addr[j].st_info) == STT_FUNC) {
                    char *symbol_name;
                    // 对于 st_name 的值不为0的符号或者 ABS, 去对应的 .strtab 中找
                    if (symtab_addr[j].st_name || symtab_addr[j].st_shndx == SHN_ABS) {
                        symbol_name =
                            (char *)((char *)ELF_file_data->addr + strtab->sh_offset + symtab_addr[j].st_name);
                    } else {
                        // 为 0 说明是一个特殊符号, 用 symbol_ndx 去段表字符串表中找
                        symbol_name = (char *)((char *)ELF_file_data->addr + ELF_file_data->shstrtab_offset +
                                               ELF_file_data->shdr[symtab_addr[j].st_shndx].sh_name);
                    }
                    symbol_table->symbols[index].addr = start_addr + (symtab_addr[j].st_value - offset);
                    // symbol_table->symbols[index].type = ELF64_ST_BIND(symtab_addr[j].st_info);
                    strcpy(symbol_table->symbols[index].symbol_name, symbol_name);
                    index++;
                }
            }
        }
    }
    symbol_table->symbols = (struct Symbol *)realloc(symbol_table->symbols, index * sizeof(struct Symbol));
    symbol_table->count = index;
    return 0;
}

/**
 * @brief 读取一个 elf 文件
 *
 * @param elf_path
 * @param symbol_table
 * @param start_addr 可执行段的虚拟地址起始位置
 * @param offset 可执行段在 ELF 文件中的偏移量
 * @return int
 */
static int load_elf_file(const char *elf_path, SymbolTable *symbol_table, unsigned long long start_addr,
                         unsigned long long offset) {
    printf("load elf file: %s\n", elf_path);
    int fd = open(elf_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    // 对 ELF 文件做完整的内存映射, 保存在 ELF_file_data.addr 中, 方便后面寻址
    off_t size = lseek(fd, 0, SEEK_END);
    void *addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }
    ELF ELF_file_data;
    ELF_file_data.addr = addr;

    // 读取 ELF 头, 保存在 ELF_file_data.ehdr 中
    lseek(fd, 0, SEEK_SET);
    if (read(fd, &ELF_file_data.ehdr, sizeof(Elf64_Ehdr)) < 0) {
        perror("read");
        munmap(addr, size);
        close(fd);
        return -1;
    }

    int section_number = ELF_file_data.ehdr.e_shnum;                       // 段的数量
    unsigned long long section_table_offset = ELF_file_data.ehdr.e_shoff;  // 段表的偏移量
    lseek(fd, section_table_offset, SEEK_SET);

    // 读取段表的所有信息, 保存在 shdr 中
    ELF_file_data.shdr = (Elf64_Shdr *)malloc(sizeof(Elf64_Shdr) * section_number);
    if (read(fd, ELF_file_data.shdr, sizeof(Elf64_Shdr) * section_number) < 0) {
        perror("read");
        munmap(addr, size);
        close(fd);
        free(ELF_file_data.shdr);
        return -1;
    }

    // 段表字符串表的索引
    int section_string_index = ELF_file_data.ehdr.e_shstrndx;

    // 段表字符串表的偏移量
    Elf64_Off shstrtab_offset = ELF_file_data.shdr[section_string_index].sh_offset;
    ELF_file_data.shstrtab_offset = shstrtab_offset;

    // 读取符号表的信息, 保存在 symbol_table 中
    load_elf_symbol_table(&ELF_file_data, symbol_table, start_addr, offset);
    close(fd);

    munmap(addr, size);
    free(ELF_file_data.shdr);
    return 0;
}

/**
 * @brief load user symbols
 *
 * @param elf_path
 * @return SymbolTable*
 */
SymbolTable *load_user_symbols(int pid) {
    // 读取内存映射 /proc/<pid>/maps, 获取用户程序的内存映射
    char memory_map_path[PATH_MAX];
    sprintf(memory_map_path, "/proc/%d/maps", pid);

    FILE *fp = fopen(memory_map_path, "r");
    if (fp == NULL) {
        return NULL;
    }

    char buffer[PATH_MAX];
    SymbolTable *symbol_table = (SymbolTable *)calloc(sizeof(SymbolTable), 1);
    unsigned long long start_addr, end_addr, offset, inode;
    char mode[5];
    char dev[10];
    char file_name[128];
    char elf_path[PATH_MAX];
    while (fgets(buffer, PATH_MAX, fp)) {
        if (sscanf(buffer,
                   "%llx-%llx %s %llx %s %llu %s",
                   &start_addr,
                   &end_addr,
                   mode,
                   &offset,
                   dev,
                   &inode,
                   file_name) == 7) {
            if (strstr(mode, "x") != NULL && file_name[0] == '/') {
                // 是一个具有执行权限的内存映射
                sprintf(elf_path, "/proc/%d/root%s", pid, file_name);
                load_elf_file(elf_path, symbol_table, start_addr, offset);
            }
        }
    }
    fclose(fp);
    // ELF elf_file_data;
    // if (load_elf_file(elf_path, &elf_file_data) < 0) {
    //     return NULL;
    // }
    // SymbolTable *symbol_table = (SymbolTable *)malloc(sizeof(SymbolTable));
    // if (load_elf_symbol_table(&elf_file_data, symbol_table) < 0) {
    //     return NULL;
    // }
    return symbol_table;
}
