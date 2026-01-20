
#pragma once

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

int is_root();
unsigned long long parse_hex(char *p, int *n);
char **parse_cmdline(const char *cmdline);
void free_argv(char **args);
int copy_file(const char *src, const char *dst);
int file_exists(const char *path);