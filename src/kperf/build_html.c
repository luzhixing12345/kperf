
#include <linux/limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>
#include "build_html.h"
#include "config.h"
#include "log.h"
#include "symbol.h"
#include "utils.h"

extern int need_kernel_callchain;
int node_id = 0;

/* lookup symbol by addr: find symbol with largest addr <= target using binary search */
const char *find_name(struct symbol_table *st, uint64_t addr) {
    if (!st || st->size == 0)
        return NULL;

    int left = 0;
    int right = st->size - 1;
    const char *best = NULL;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        uint64_t mid_addr = st->symbols[mid].addr;

        if (mid_addr == addr) {
            // exact match
            return st->symbols[mid].name;
        } else if (mid_addr < addr) {
            best = st->symbols[mid].name;  // mid_addr <= target, candidate
            left = mid + 1;                // try to find larger addr <= target
        } else {
            right = mid - 1;  // mid_addr > target, go left
        }
    }

    return best;
}

/* add name under node and return child node */
struct node *node_add(struct node *cur, const char *name, uint32_t pid, uint32_t tid) {
    if (!cur)
        return NULL;
    // cur->count++;
    struct child *it = cur->children;
    while (it) {
        if (!strcmp(it->name, name) && it->n->pid == pid && it->n->tid == tid) {
            // it->n->count++;
            return it->n;
        }
        it = it->next;
    }
    struct child *nc = malloc(sizeof(*nc));
    nc->name = strdup(name);
    nc->n = calloc(1, sizeof(*nc->n));
    nc->n->count = 0;
    nc->n->pid = pid;
    nc->n->tid = tid;
    nc->n->node_id = node_id++;
    nc->next = cur->children;
    cur->children = nc;
    return nc->n;
}

void node_free(struct node *n) {
    if (!n)
        return;
    struct child *c = n->children;
    while (c) {
        struct child *next = c->next;
        node_free(c->n);
        free(c->name);
        free(c);
        c = next;
    }

    free(n);
}

/*
 * 1. pid, smaller first
 * 2. tid, smaller first
 * 3. count, larger first
 * 4. name, lexicographically
 */
int cmp_child(const void *a, const void *b) {
    struct child *ca = *(struct child **)a;
    struct child *cb = *(struct child **)b;
    // first compare pid
    if (ca->n->pid < cb->n->pid) {
        return -1;
    } else if (ca->n->pid > cb->n->pid) {
        return 1;
    }
    // then compare tid
    if (ca->n->tid < cb->n->tid) {
        return -1;
    } else if (ca->n->tid > cb->n->tid) {
        return 1;
    }
    // then compare count
    if (ca->n->count > cb->n->count) {
        return -1;
    }
    if (ca->n->count < cb->n->count)
        return 1;
    // then compare name
    return strcmp(ca->name, cb->name);
}

int print_node_html(FILE *fp, struct node *n, int k) {
    if (!n)
        return k;
    /* count children */
    int cnt = 0;
    for (struct child *c = n->children; c; c = c->next) cnt++;
    if (cnt == 0)
        return k;
    /* collect */
    struct child **arr = malloc(sizeof(struct child *) * cnt);
    int idx = 0;
    for (struct child *c = n->children; c; c = c->next) arr[idx++] = c;
    /* sort by count desc */

    qsort(arr, cnt, sizeof(struct child *), cmp_child);

    for (int i = 0; i < cnt; i++) {
        struct child *c = arr[i];
        int count = c->n->count;
        double pct = 100.0 * count / (n->count ? n->count : 1);
        // if (pct < MIN_SHOW_PERCENT) {
        //     continue;
        // }
        fprintf(fp, "<li>\n");
        fprintf(fp, "<input type=\"checkbox\" id=\"c%d\" />\n", c->n->node_id);
        fprintf(fp,
                "<label class=\"tree_label\" for=\"c%d\">%s(%.1f%% %d/%ld)",
                c->n->node_id,
                c->name,
                pct,
                count,
                n->count);
        if (k == 0 && c->n->pid != c->n->tid) {
            fprintf(fp, " [pid: %d, tid: %d]</label>\n", c->n->pid, c->n->tid);
        } else {
            fprintf(fp, "</label>\n");
        }
        fprintf(fp, "<ul>\n");
        print_node_html(fp, c->n, k + 1);
        fprintf(fp, "</ul>\n");
        fprintf(fp, "</li>\n");
    }
    free(arr);
    return k;
}

struct node *build_tree(struct perf_sample_table *pst, struct symbol_table *ust, struct symbol_table *kst) {
    struct node *root = calloc(1, sizeof(*root));
    root->count = 0;
    root->node_id = node_id++;

    /* build tree from pst samples */
    for (int i = 0; i < pst->size; i++) {
        struct perf_sample *s = &pst->samples[i];
        if (!s || s->nr == 0 || !s->ips)
            continue;
        struct node *r = root;
        r->count++;
        /* iterate callchain from last to first */
        for (int j = (int)s->nr - 1; j >= 0; j--) {
            uint64_t addr = s->ips[j];
            uint32_t pid = s->pid;
            uint32_t tid = s->tid;
            const char *name = NULL;
            if (addr >= 0xfffffffffffffe00) {
                // https://wolf.nereid.pl/posts/perf-stack-traces/
                continue;
            }
            if (kst && need_kernel_callchain)
                name = find_name(kst, addr);
            if (!name && ust)
                name = find_name(ust, addr);

            char hexbuf[64];
            if (!name) {
                WARNING("no name found 0x%lx\n", addr);
                snprintf(hexbuf, sizeof(hexbuf), "0x%lx", (unsigned long)addr);
                name = hexbuf;
            }
            r = node_add(r, name, pid, tid);
            r->count++;
        }
    }
    return root;
}

int build_html(struct perf_sample_table *pst, struct symbol_table *ust, struct symbol_table *kst) {
    if (!pst || pst->size == 0) {
        ERR("no samples to build html\n");
        return -1;
    }

    /* build tree */
    struct node *root = build_tree(pst, ust, kst);

    /* render tree into memory buffer */
    char *tree_buf = NULL;
    size_t tree_size = 0;
    FILE *ms = open_memstream(&tree_buf, &tree_size);
    if (!ms) {
        perror("open_memstream");
        return -1;
    }

    /* printing: collect children into array and sort by count */
    INFO("building html report\n");
    fprintf(ms, "<ul class=\"tree\">\n");
    print_node_html(ms, root, 0);
    fprintf(ms, "</ul>\n");
    fclose(ms);

    /* read template assets/index.html */
    const char *tpl_path = KPERF_ETC_TPL_PATH "/index.html";  // for local development
    FILE *tf = fopen(tpl_path, "r");
    if (!tf) {
        ERR("failed to open %s: %s\n", tpl_path, strerror(errno));
        return -1;
    }

    fseek(tf, 0, SEEK_END);
    long tpl_len = ftell(tf);
    fseek(tf, 0, SEEK_SET);
    char *tpl = malloc(tpl_len + 1);
    if (!tpl) {
        perror("malloc tpl");
        fclose(tf);
        free(tree_buf);
        return -1;
    }
    if (fread(tpl, 1, tpl_len, tf) != (size_t)tpl_len) {
        perror("fread tpl");
        fclose(tf);
        free(tpl);
        free(tree_buf);
        return -1;
    }
    tpl[tpl_len] = '\0';
    fclose(tf);

    /* find placeholder and replace */
    const char *placeholder = "$function-tree";
    char *pos = strstr(tpl, placeholder);
    if (!pos) {
        WARNING("placeholder %s not found in %s\n", placeholder, tpl_path);
    }

    /* ensure output directory exists */
    if (mkdir(KPERF_RESULTS_PATH, 0755) < 0 && errno != EEXIST) {
        WARNING("mkdir failed: %s\n", strerror(errno));
    }

    char index_path[PATH_MAX];
    snprintf(index_path, sizeof(index_path), "%s/index.html", KPERF_RESULTS_PATH);
    FILE *of = fopen(index_path, "w");
    if (!of) {
        perror("fopen output report");
        free(tpl);
        free(tree_buf);
        return -1;
    }

    if (pos) {
        /* write content before placeholder */
        size_t prefix_len = pos - tpl;
        fwrite(tpl, 1, prefix_len, of);
        /* write tree buffer */
        fwrite(tree_buf, 1, tree_size, of);
        /* write rest */
        fwrite(pos + strlen(placeholder), 1, tpl_len - prefix_len - strlen(placeholder), of);
    } else {
        /* no placeholder: write template then tree */
        fwrite(tpl, 1, tpl_len, of);
        fwrite(tree_buf, 1, tree_size, of);
    }

    fclose(of);
    free(tpl);
    free(tree_buf);
    node_free(root);

    /* copy js/css/svg to kperf-result */
    char *assets[] = {
        KPERF_ETC_TPL_PATH "/index.js", KPERF_ETC_TPL_PATH "/index.css", KPERF_ETC_TPL_PATH "/favicon.svg"};
    for (int i = 0; i < sizeof(assets) / sizeof(*assets); i++) {
        char *src = assets[i];
        char dst[PATH_MAX];
        snprintf(dst, sizeof(dst), "%s/%s", KPERF_RESULTS_PATH, basename(src));
        if (copy_file(src, dst) < 0) {
            WARNING("failed to copy %s to %s: %s\n", src, dst, strerror(errno));
        }
    }
    return 0;
}