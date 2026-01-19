
#pragma once

#include "perf.h"
#include "symbol.h"

/* tree node */
struct child {
    char *name;
    struct node *n;
    struct child *next;
};
struct node {
    long c;
    struct child *children;
};

void build_html(struct perf_sample_table *pst, struct symbol_table *ust, struct symbol_table *kst);