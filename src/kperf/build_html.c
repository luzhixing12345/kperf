
#define _GNU_SOURCE
#include <linux/limits.h>
#include <stdint.h>
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
struct symbol *find_symbol(struct symbol_table *st, uint64_t addr) {
    if (!st || st->size == 0)
        return NULL;

    int left = 0;
    int right = st->size - 1;
    struct symbol *best = NULL;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        uint64_t mid_addr = st->symbols[mid].addr;

        if (mid_addr == addr) {
            // exact match
            return &st->symbols[mid];
        } else if (mid_addr < addr) {
            best = &st->symbols[mid];  // mid_addr <= target, candidate
            left = mid + 1;            // try to find larger addr <= target
        } else {
            right = mid - 1;  // mid_addr > target, go left
        }
    }

    return best;
}

/* add name under node and return child node */
struct node *node_add(struct node *cur, struct symbol *sym, uint64_t retback_addr, uint32_t pid, uint32_t tid) {
    if (!cur)
        return NULL;

    // printf("ret_addr = %lu\n", ret_addr);
    // struct child *it = cur->children;
    struct node *c = NULL;
    for (int i = 0; i < cur->nr_children; i++) {
        c = &cur->children[i];
        if (!strcmp(c->sym->name, sym->name) && c->retback_addr == retback_addr && c->pid == pid && c->tid == tid) {
            return c;
        }
    }

    cur->nr_children++;
    cur->children = realloc(cur->children, sizeof(struct node) * cur->nr_children);
    c = &cur->children[cur->nr_children - 1];
    memset(c, 0, sizeof(struct node));
    c->sym = sym;
    c->retback_addr = retback_addr;
    c->count = 0;
    c->pid = pid;
    c->tid = tid;
    c->node_id = node_id++;
    return c;
}

void node_free(struct node *n) {
    if (!n)
        return;

    for (int i = 0; i < n->nr_children; i++) {
        struct node *c = &n->children[i];
        node_free(c);
    }
    free(n->children);
}

/*
 * 1. pid, smaller first
 * 2. tid, smaller first
 * 3. ret_addr, smaller first
 * 3. count, larger first
 * 4. name, lexicographically
 */
int cmp_child(const void *a, const void *b) {
    struct node *ca = (struct node *)a;
    struct node *cb = (struct node *)b;
    // first compare pid
    if (ca->pid < cb->pid) {
        return -1;
    } else if (ca->pid > cb->pid) {
        return 1;
    }
    // then compare tid
    if (ca->tid < cb->tid) {
        return -1;
    } else if (ca->tid > cb->tid) {
        return 1;
    }
    // then compare ret_addr
    if (ca->retback_addr < cb->retback_addr) {
        return -1;
    } else if (ca->retback_addr > cb->retback_addr) {
        return 1;
    }
    // then compare count
    if (ca->count > cb->count) {
        return -1;
    }
    if (ca->count < cb->count)
        return 1;
    // then compare name
    return strcmp(ca->sym->name, cb->sym->name);
}

int print_node_html(FILE *fp, struct node *n, int k) {
    if (!n)
        return k;

    qsort(n->children, n->nr_children, sizeof(struct node), cmp_child);

    for (int i = 0; i < n->nr_children; i++) {
        struct node *c = &n->children[i];
        int count = c->count;
        double pct = 100.0 * count / (n->count ? n->count : 1);
        // if (pct < MIN_SHOW_PERCENT) {
        //     continue;
        // }
        fprintf(fp, "<li>\n");
        fprintf(fp, "<input type=\"checkbox\" id=\"c%d\" />\n", c->node_id);
        fprintf(fp,
                "<label class=\"tree_label\" for=\"c%d\" text=\"%s %s:%d\">%s(%.1f%% %d/%ld)",
                c->node_id,
                c->sym->module ? c->sym->module : "?",
                c->sym->filename ? c->sym->filename : "?",
                c->sym->lineno,
                c->sym->name,
                pct,
                count,
                n->count);
        if (k == 0 && c->pid != c->tid) {
            fprintf(fp, " [pid: %d, tid: %d]</label>\n", c->pid, c->tid);
        } else {
            fprintf(fp, "</label>\n");
        }
        fprintf(fp, "<ul>\n");
        print_node_html(fp, c, k + 1);
        fprintf(fp, "</ul>\n");
        fprintf(fp, "</li>\n");
    }
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
        uint64_t retback_addr = 0;
        /* iterate callchain from last to first */
        for (int j = (int)s->nr - 1; j >= 0; j--) {
            uint64_t addr = s->ips[j];
            uint32_t pid = s->pid;
            uint32_t tid = s->tid;
            struct symbol *sym = NULL;
            // const char *name = NULL;
            uint64_t func_retback_addr = 0;
            if (addr >= 0xfffffffffffffe00) {
                // https://wolf.nereid.pl/posts/perf-stack-traces/
                continue;
            }
            if (kst && need_kernel_callchain)
                sym = find_symbol(kst, addr);
            if (!sym && ust)
                sym = find_symbol(ust, addr);

            // char hexbuf[64];
            if (!sym) {
                WARNING("no name found 0x%lx\n", addr);
                // snprintf(hexbuf, sizeof(hexbuf), "0x%lx", (unsigned long)addr);
                // name = hexbuf;
                // func_retback_addr = 0;
                continue;
            } else {
                // name = sym->name;
                // https://wolf.nereid.pl/posts/perf-stack-traces/
                // j == 0 ip has no meanning, the same as (addr >= 0xfffffffffffffe00)
                func_retback_addr = j == 1 ? 0 : addr - sym->addr;
            }
            /*
             * there maybe multiple function call with the same name like below
             * kperf should be able to distinguish each function call instead of
             * intigrate them into one node
             *
             * int main() {
             *     foo();
             *     foo();
             * }
             *
             * perf_sample ips look like
             *
             * [first foo]   [second foo]
             * - main+0x24   - main+0x44
             * - foo         - foo
             *
             * we use retback_addr to distinguish each function call.
             * retback_addr is the return address of foo(), so it should be passed to node_add()
             * in the next iteration.
             *
             * NOTE: see the result of test/thread/1p1t
             */
            r = node_add(r, sym, retback_addr, pid, tid);
            r->count++;
            retback_addr = func_retback_addr;
        }
    }
    return root;
}

int build_html(struct perf_sample_table *pst, struct symbol_table *ust, struct symbol_table *kst) {
    struct node *root = NULL;
    char *tree_buf = NULL;
    char *css = NULL;
    char *js = NULL;
    char *tpl = NULL;
    char *result = NULL;
    char *css_replacement = NULL;
    char *js_replacement = NULL;
    size_t tree_size = 0;
    int ret = -1;

    if (!pst || pst->size == 0) {
        ERR("no samples to build html\n");
        goto cleanup;
    }

    /* build tree */
    root = build_tree(pst, ust, kst);
    if (!root) {
        ERR("failed to build tree\n");
        goto cleanup;
    }

    /* render tree into memory buffer */
    FILE *ms = open_memstream(&tree_buf, &tree_size);
    if (!ms) {
        perror("open_memstream");
        goto cleanup;
    }

    INFO("building html report\n");
    fprintf(ms, "<ul class=\"tree\">\n");
    print_node_html(ms, root, 0);
    fprintf(ms, "</ul>\n");
    fclose(ms);

    /* read index.css */
    css = read_file_content(KPERF_ETC_TPL_PATH "/index.css");
    if (!css) {
        ERR("failed to read index.css\n");
        goto cleanup;
    }

    /* read index.js */
    js = read_file_content(KPERF_ETC_TPL_PATH "/index.js");
    if (!js) {
        ERR("failed to read index.js\n");
        goto cleanup;
    }

    /* read template assets/index.html */
    const char *tpl_path = KPERF_ETC_TPL_PATH "/index.html";
    tpl = read_file_content(tpl_path);
    if (!tpl) {
        ERR("failed to open %s: %s\n", tpl_path, strerror(errno));
        ERR("probably you use `make release` to build kperf, but release mode is only used for debian pkg\n");
        ERR("please use `make` to build kperf for local development(assets/index.html is what you need)\n");
        goto cleanup;
    }

    /* replace $function-tree with tree content */
    result = replace_string(tpl, "$function-tree", tree_buf);
    if (!result) {
        ERR("failed to replace $function-tree\n");
        goto cleanup;
    }

    /* replace $css with actual CSS content wrapped in <style> tags */
    if (asprintf(&css_replacement, "<style>\n%s\n</style>", css) < 0) {
        ERR("failed to allocate css replacement\n");
        goto cleanup;
    }
    char *new_result = replace_string(result, "$css", css_replacement);
    if (!new_result) {
        ERR("failed to replace $css\n");
        goto cleanup;
    }
    result = new_result;

    /* replace $js with actual JS content wrapped in <script> tags */
    if (asprintf(&js_replacement, "<script>\n%s\n</script>", js) < 0) {
        ERR("failed to allocate js replacement\n");
        goto cleanup;
    }
    new_result = replace_string(result, "$js", js_replacement);
    if (!new_result) {
        ERR("failed to replace $js\n");
        goto cleanup;
    }
    result = new_result;

    /* write final result */
    char *index_path = KPERF_RESULT_PATH;
    FILE *wf = fopen(index_path, "w");
    if (!wf) {
        ERR("failed to open %s for writing: %s\n", index_path, strerror(errno));
        goto cleanup;
    }
    fwrite(result, 1, strlen(result), wf);
    fclose(wf);
    chmod(index_path, 0666);
    ret = 0;

cleanup:
    free(css_replacement);
    free(js_replacement);
    free(result);
    free(css);
    free(js);
    free(tpl);
    free(tree_buf);
    if (root) {
        node_free(root);
        free(root);
    }
    return ret;
}