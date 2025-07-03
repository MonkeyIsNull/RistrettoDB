#ifndef RISTRETTO_BTREE_H
#define RISTRETTO_BTREE_H

#include <stdint.h>
#include <stdbool.h>
#include "pager.h"
#include "storage.h"

#define BTREE_ORDER 255
#define BTREE_MIN_KEYS ((BTREE_ORDER - 1) / 2)

typedef struct {
    uint32_t page_num;
    bool is_leaf;
    uint32_t num_keys;
    uint32_t parent;
    uint32_t keys[BTREE_ORDER - 1];
    union {
        uint32_t children[BTREE_ORDER];
        RowId values[BTREE_ORDER - 1];
    } ptrs;
} BTreeNode;

typedef struct {
    Pager *pager;
    uint32_t root_page;
    Table *table;
} BTree;

BTree* btree_create(Pager *pager, Table *table);
void btree_destroy(BTree *btree);

bool btree_insert(BTree *btree, uint32_t key, RowId value);
RowId* btree_find(BTree *btree, uint32_t key);
bool btree_delete(BTree *btree, uint32_t key);

typedef struct {
    BTree *btree;
    uint32_t page_num;
    uint32_t cell_num;
    bool end_of_table;
} BTreeCursor;

BTreeCursor* btree_cursor_create(BTree *btree);
void btree_cursor_destroy(BTreeCursor *cursor);

void btree_cursor_first(BTreeCursor *cursor);
void btree_cursor_advance(BTreeCursor *cursor);
bool btree_cursor_at_end(BTreeCursor *cursor);

uint32_t btree_cursor_key(BTreeCursor *cursor);
RowId btree_cursor_value(BTreeCursor *cursor);

#endif
