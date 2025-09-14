#ifndef SEGMENT_TREE_H
#define SEGMENT_TREE_H

typedef struct
{
    int n;     /* number of leaves */
    int *tree; /* 1-indexed array, size ~ 4*n, stores aggregated sums */
} segment_tree_t;

/* Build a tree for n leaves initialized from base[] where base[i] is the leaf
 * value at i in [0..n-1]. */
int st_build(segment_tree_t *st, const int *base, int n);

/* Free memory associated with the tree. */
void st_free(segment_tree_t *st);

/* Point update: set leaf index i to value val, and update ancestors. */
void st_update(segment_tree_t *st, int i, int val);

/* Query total sum (useful to check if any leaf > 0). */
int st_total(const segment_tree_t *st);

/* Find leftmost index with leaf value > 0; returns -1 if none. */
int st_find_leftmost_positive(const segment_tree_t *st);

#endif /* SEGMENT_TREE_H */
