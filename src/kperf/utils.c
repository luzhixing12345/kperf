
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "utils.h"
#include "log.h"

int is_root() {
    return geteuid() == 0;
}

unsigned long long parse_hex(char *p, int *n) {
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

char **parse_cmdline(const char *cmdline) {
    if (!cmdline) {
        return NULL;
    }

    int argc = 0;
    int capacity = 8;
    char **argv = malloc(capacity * sizeof(char *));
    if (!argv) {
        return NULL;
    }

    const char *p = cmdline;

    while (*p) {
        /* 跳过前导空白 */
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (!*p) {
            break;
        }

        /* 记录 token 起点 */
        const char *start = p;

        while (*p && !isspace((unsigned char)*p)) {
            p++;
        }

        size_t len = p - start;
        char *arg = malloc(len + 1);
        if (!arg) {
            goto fail;
        }

        memcpy(arg, start, len);
        arg[len] = '\0';

        /* 扩容 */
        if (argc + 1 >= capacity) {
            capacity *= 2;
            char **new_argv = realloc(argv, capacity * sizeof(char *));
            if (!new_argv) {
                free(arg);
                goto fail;
            }
            argv = new_argv;
        }

        argv[argc++] = arg;
    }

    argv[argc] = NULL;
    return argv;

fail:
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
    return NULL;
}

void free_argv(char **argv) {
    if (!argv) {
        return;
    }
    for (int i = 0; argv[i]; i++) {
        free(argv[i]);
    }
    free(argv);
}

int copy_file(const char *src, const char *dst) {
    FILE *sf = fopen(src, "r");
    if (!sf) {
        ERR("failed to open %s: %s\n", src, strerror(errno));
        return -1;
    }
    FILE *df = fopen(dst, "w");
    if (!df) {
        ERR("failed to open %s: %s\n", dst, strerror(errno));
        fclose(sf);
        return -1;
    }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), sf)) > 0) {
        if (fwrite(buf, 1, n, df) != n) {
            ERR("failed to write to %s: %s\n", dst, strerror(errno));
            fclose(sf);
            fclose(df);
            return -1;
        }
    }
    fclose(sf);
    fclose(df);
    DEBUG("copied %s to %s\n", src, dst);
    return 0;
}
