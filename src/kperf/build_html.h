
#pragma once

#include "perf.h"
#include "symbol.h"

#define MIN_SHOW_PERCENT 1

/* tree node */
struct child {
    char *name;
    struct node *n;
    struct child *next;
    uint32_t pid;
    uint32_t tid;
};
struct node {
    long count;
    struct child *children;
    uint32_t pid;
    uint32_t tid;
};

struct node *build_tree(struct perf_sample_table *pst, struct symbol_table *ust, struct symbol_table *kst);
void node_free(struct node *n);
void build_html(struct perf_sample_table *pst, struct symbol_table *ust, struct symbol_table *kst);
int cmp_child(const void *a, const void *b);