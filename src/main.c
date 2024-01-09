
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <elf.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "parse_symbol.h"
#include "xargparse.h"
#include "xutils.h"

static int pid = 0;
static int gid = 0;
static char* program_name = NULL;
static char** program_args = NULL;

int main(int argc, const char** argv) {
    argparse_option options[] = {XBOX_ARG_BOOLEAN(NULL, NULL, "--help", "show help information", NULL, "help"),
                                 XBOX_ARG_STR_GROUP(&program_name, NULL, NULL, "program name to run", NULL, "program"),
                                 XBOX_ARG_STRS_GROUP(&program_args, NULL, NULL, "program args", NULL, "args"),
                                 XBOX_ARG_INT(&pid, NULL, "--pid", "a running pid", " [PID]", "pid"),
                                 XBOX_ARG_INT(&gid, NULL, "--gid", "a running gid", " [GID]", "gid"),
                                 XBOX_ARG_END()};

    XBOX_argparse parser;
    XBOX_argparse_init(&parser, options, XBOX_ARGPARSE_IGNORE_UNKNOWN | XBOX_ARGPARSE_ENABLE_EQUAL);
    XBOX_argparse_describe(&parser,
                           "kperf",
                           "kperf - a simple kernel performance monitor",
                           "\nonline usage: https://github.com/luzhixing12345/kperf");
    XBOX_argparse_parse(&parser, argc, argv);

    if (XBOX_match_pos(&parser, "help")) {
        XBOX_argparse_info(&parser);
        XBOX_free_argparse(&parser);
        return 0;
    }

    char elf_path[PATH_MAX];
    if (program_name == NULL) {
        if (pid == 0) {
            fprintf(stderr, "no program name or pid\n");
            XBOX_argparse_info(&parser);
            XBOX_free_argparse(&parser);
            return -1;
        } else {
            // char proc_exe_path[PATH_MAX];
            // snprintf(proc_exe_path, sizeof(proc_exe_path), "/proc/%d/exe", pid);
            // ssize_t r = readlink(proc_exe_path, elf_path, sizeof(elf_path) - 1);
            // if (r == -1) {
            //     perror("readlink");
            //     XBOX_free_argparse(&parser);
            //     return -1;
            // }
            // elf_path[r] = '\0';
        }
    } else {
        printf("program name: %s\n", program_name);
        // fork and exec
        // if (fork() == 0) {
        //     execvp(program_name, program_args);
        // }
        strncpy(elf_path, program_name, PATH_MAX);
        XBOX_free_argparse(&parser);
        printf("not support yet\n");
        return 0;
    }

    SymbolTable* user_symbol_table = load_user_symbols(pid);
    if (user_symbol_table == NULL) {
        fprintf(stderr, "fail to load user symbols\n");
        XBOX_free_argparse(&parser);
        return -1;
    }
    print_symbol_table(user_symbol_table);
    free_symbol_table(user_symbol_table);

    // SymbolTable* kernel_symbol_table = load_kernel_symbols();
    // if (kernel_symbol_table == NULL) {
    //     fprintf(stderr, "fail to load kernel symbols\n");
    //     return -1;
    // }
    // print_symbol_table(kernel_symbol_table);
    // free_symbol_table(kernel_symbol_table);
    XBOX_free_argparse(&parser);
    return 0;
}
