
#include <stdio.h>
#include <stdlib.h>

#include "build_html.h"

int print_node_tui(struct node *n, int k) {
    static int prefix[64];  // 记录每一层是否还有兄弟
    if (!n)
        return k;

    qsort(n->children, n->nr_children, sizeof(struct node), cmp_child);

    for (int i = 0; i < n->nr_children; i++) {
        struct node *c = &n->children[i];
        int is_last = (i == n->nr_children - 1);
        /* 当前分支符号 */
        int count = c->count;
        double pct = 100.0 * count / (n->count ? n->count : 1);
        // if (pct < MIN_SHOW_PERCENT) {
        //     continue;
        // }

        /* 打印 tree 前缀 */
        for (int d = 0; d < k; d++) {
            if (prefix[d])
                printf("│ ");
            else
                printf("  ");
        }

        if (k == 0) {
            printf("%s", c->name);
        } else
            printf("%s── %s", is_last ? "└" : "├", c->name);

        printf(" (%.1f%% %d/%ld)", pct, count, n->count);

        if (k == 0 && c->pid != c->tid) {
            printf("[pid: %d, tid: %d]\n", c->pid, c->tid);
        } else {
            printf("\n");
        }

        /* 记录这一层是否还有兄弟 */
        prefix[k] = !is_last;

        print_node_tui(c, k + 1);
    }

    return k;
}

void build_tui(struct perf_sample_table *pst, struct symbol_table *ust, struct symbol_table *kst) {
    struct node *root = build_tree(pst, ust, kst);
    print_node_tui(root, 0);
    node_free(root);
    free(root);
}