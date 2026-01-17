
#pragma once

int is_root();
unsigned long long parse_hex(char *p, int *n);
char **parse_cmdline(const char *cmdline);
void free_argv(char **args);