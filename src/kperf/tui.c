
#include <stdio.h>
#include <stdlib.h>

#include "build_html.h"
#include "log.h"

int print_node_tui(struct node *n, int k) {
    static int prefix[64];  // 记录每一层是否还有兄弟
    if (!n)
        return k;

    /* count children */
    int cnt = 0;
    for (struct child *c = n->children; c; c = c->next) cnt++;

    if (cnt == 0)
        return k;

    /* collect children */
    struct child **arr = malloc(sizeof(struct child *) * cnt);
    int idx = 0;
    for (struct child *c = n->children; c; c = c->next) arr[idx++] = c;

    for (int i = 0; i < cnt; i++) {
        struct child *c = arr[i];
        int is_last = (i == cnt - 1);
        /* 当前分支符号 */
        int count = c->n->count;
        double pct = 100.0 * count / (n->count ? n->count : 1);
        if (pct < MIN_SHOW_PERCENT) {
            continue;
        }

        /* 打印 tree 前缀 */
        for (int d = 0; d < k; d++) {
            if (prefix[d])
                printf("│   ");
            else
                printf("    ");
        }

        printf("%s── %s", is_last ? "└" : "├", c->name);

        printf(" (%.1f%% %d/%ld)\n", pct, count, n->count);

        /* 记录这一层是否还有兄弟 */
        prefix[k] = !is_last;

        print_node_tui(c->n, k + 1);
    }

    free(arr);
    return k;
}

void build_tui(struct perf_sample_table *pst, struct symbol_table *ust, struct symbol_table *kst) {
    struct node *root = build_tree(pst, ust, kst);
    print_node_tui(root, 0);
    node_free(root);
}