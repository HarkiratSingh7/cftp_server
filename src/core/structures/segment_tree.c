#include "segment_tree.h"

#include <stdlib.h>
#include <string.h>

static void build_rec(segment_tree_t *st,
                      const int *base,
                      int v,
                      int tl,
                      int tr)
{
    if (tl == tr)
    {
        st->tree[v] = base ? base[tl] : 0;
        return;
    }
    int tm = (tl + tr) / 2;
    build_rec(st, base, v * 2, tl, tm);
    build_rec(st, base, v * 2 + 1, tm + 1, tr);
    st->tree[v] = st->tree[v * 2] + st->tree[v * 2 + 1];
}

int st_build(segment_tree_t *st, const int *base, int n)
{
    if (n <= 0) return -1;
    st->n = n;
    st->tree = (int *)calloc((size_t)(4 * n), sizeof(int));
    if (!st->tree) return -1;
    build_rec(st, base, 1, 0, n - 1);
    return 0;
}

void st_free(segment_tree_t *st)
{
    if (!st) return;
    free(st->tree);
    st->tree = NULL;
    st->n = 0;
}

static void update_rec(segment_tree_t *st,
                       int v,
                       int tl,
                       int tr,
                       int i,
                       int val)
{
    if (tl == tr)
    {
        st->tree[v] = val;
        return;
    }
    int tm = (tl + tr) / 2;
    if (i <= tm)
        update_rec(st, v * 2, tl, tm, i, val);
    else
        update_rec(st, v * 2 + 1, tm + 1, tr, i, val);
    st->tree[v] = st->tree[v * 2] + st->tree[v * 2 + 1];
}

void st_update(segment_tree_t *st, int i, int val)
{
    if (!st || !st->tree || i < 0 || i >= st->n) return;
    update_rec(st, 1, 0, st->n - 1, i, val);
}

int st_total(const segment_tree_t *st)
{
    if (!st || !st->tree) return 0;
    return st->tree[1];
}

static int find_leftmost_pos_rec(const segment_tree_t *st,
                                 int v,
                                 int tl,
                                 int tr)
{
    if (st->tree[v] == 0) return -1;
    if (tl == tr) return tl;
    int tm = (tl + tr) / 2;
    if (st->tree[v * 2] > 0) return find_leftmost_pos_rec(st, v * 2, tl, tm);
    return find_leftmost_pos_rec(st, v * 2 + 1, tm + 1, tr);
}

int st_find_leftmost_positive(const segment_tree_t *st)
{
    if (!st || !st->tree) return -1;
    return find_leftmost_pos_rec(st, 1, 0, st->n - 1);
}
