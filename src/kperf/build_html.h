
#pragma once

#include <stdint.h>
#include "perf.h"
#include "symbol.h"

#define MIN_SHOW_PERCENT 1

/* tree node */
struct node {
    long count;
    struct symbol *sym;
    uint32_t pid;
    uint32_t tid;
    uint32_t node_id;
    uint64_t retback_addr;
    struct node *children;
    int nr_children;
};

struct node *build_tree(struct perf_sample_table *pst, struct symbol_table *ust, struct symbol_table *kst);
void node_free(struct node *n);
int build_html(struct perf_sample_table *pst, struct symbol_table *ust, struct symbol_table *kst);
int cmp_child(const void *a, const void *b);