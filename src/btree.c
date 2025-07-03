#include "btree.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define BTREE_NODE_TYPE_LEAF 0x0F
#define BTREE_NODE_TYPE_INTERNAL 0x0E

typedef struct {
    uint8_t node_type;
    uint8_t is_root;
    uint32_t parent_page_num;
    uint32_t num_keys;
} NodeHeader;

static void* get_node(BTree* btree, uint32_t page_num) {
    return pager_get_page(btree->pager, page_num);
}

static NodeHeader* get_node_header(void* node) {
    return (NodeHeader*)node;
}

static uint32_t* get_node_keys(void* node) {
    return (uint32_t*)((uint8_t*)node + sizeof(NodeHeader));
}

static uint32_t* get_internal_node_children(void* node) {
    NodeHeader* header = get_node_header(node);
    return (uint32_t*)((uint8_t*)node + sizeof(NodeHeader) + 
                       sizeof(uint32_t) * (BTREE_ORDER - 1));
}

static RowId* get_leaf_node_values(void* node) {
    return (RowId*)((uint8_t*)node + sizeof(NodeHeader) + 
                    sizeof(uint32_t) * (BTREE_ORDER - 1));
}

static void initialize_node(void* node, bool is_leaf) {
    NodeHeader* header = get_node_header(node);
    header->node_type = is_leaf ? BTREE_NODE_TYPE_LEAF : BTREE_NODE_TYPE_INTERNAL;
    header->is_root = 0;
    header->parent_page_num = 0;
    header->num_keys = 0;
}

static uint32_t find_child_index(void* node, uint32_t key) {
    NodeHeader* header = get_node_header(node);
    uint32_t* keys = get_node_keys(node);
    
    uint32_t left = 0;
    uint32_t right = header->num_keys;
    
    while (left < right) {
        uint32_t mid = (left + right) / 2;
        if (keys[mid] >= key) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }
    
    return left;
}

BTree* btree_create(Pager* pager, Table* table) {
    BTree* btree = malloc(sizeof(BTree));
    if (!btree) {
        return NULL;
    }
    
    btree->pager = pager;
    btree->table = table;
    
    // Create root page
    btree->root_page = pager_allocate_page(pager);
    void* root = get_node(btree, btree->root_page);
    initialize_node(root, true);
    
    NodeHeader* header = get_node_header(root);
    header->is_root = 1;
    
    return btree;
}

void btree_destroy(BTree* btree) {
    free(btree);
}

static bool leaf_node_insert(BTree* btree, void* node, uint32_t key, RowId value) {
    NodeHeader* header = get_node_header(node);
    uint32_t* keys = get_node_keys(node);
    RowId* values = get_leaf_node_values(node);
    
    if (header->num_keys >= BTREE_ORDER - 1) {
        return false; // Node is full
    }
    
    uint32_t index = find_child_index(node, key);
    
    // Check for duplicate key
    if (index < header->num_keys && keys[index] == key) {
        return false;
    }
    
    // Shift keys and values to make room
    if (index < header->num_keys) {
        memmove(&keys[index + 1], &keys[index], 
                sizeof(uint32_t) * (header->num_keys - index));
        memmove(&values[index + 1], &values[index], 
                sizeof(RowId) * (header->num_keys - index));
    }
    
    keys[index] = key;
    values[index] = value;
    header->num_keys++;
    
    return true;
}

bool btree_insert(BTree* btree, uint32_t key, RowId value) {
    void* root = get_node(btree, btree->root_page);
    NodeHeader* header = get_node_header(root);
    
    if (header->node_type == BTREE_NODE_TYPE_LEAF) {
        return leaf_node_insert(btree, root, key, value);
    }
    
    // TODO: Implement internal node traversal and splitting
    return false;
}

static RowId* leaf_node_find(void* node, uint32_t key) {
    NodeHeader* header = get_node_header(node);
    uint32_t* keys = get_node_keys(node);
    RowId* values = get_leaf_node_values(node);
    
    uint32_t index = find_child_index(node, key);
    
    if (index < header->num_keys && keys[index] == key) {
        return &values[index];
    }
    
    return NULL;
}

RowId* btree_find(BTree* btree, uint32_t key) {
    void* root = get_node(btree, btree->root_page);
    NodeHeader* header = get_node_header(root);
    
    if (header->node_type == BTREE_NODE_TYPE_LEAF) {
        return leaf_node_find(root, key);
    }
    
    // TODO: Implement internal node traversal
    return NULL;
}


BTreeCursor* btree_cursor_create(BTree* btree) {
    BTreeCursor* cursor = malloc(sizeof(BTreeCursor));
    if (!cursor) {
        return NULL;
    }
    
    cursor->btree = btree;
    cursor->page_num = btree->root_page;
    cursor->cell_num = 0;
    cursor->end_of_table = false;
    
    return cursor;
}

void btree_cursor_destroy(BTreeCursor* cursor) {
    free(cursor);
}

void btree_cursor_first(BTreeCursor* cursor) {
    cursor->page_num = cursor->btree->root_page;
    cursor->cell_num = 0;
    
    void* node = get_node(cursor->btree, cursor->page_num);
    NodeHeader* header = get_node_header(node);
    
    // Find leftmost leaf
    while (header->node_type == BTREE_NODE_TYPE_INTERNAL) {
        uint32_t* children = get_internal_node_children(node);
        cursor->page_num = children[0];
        node = get_node(cursor->btree, cursor->page_num);
        header = get_node_header(node);
    }
    
    cursor->cell_num = 0;
    cursor->end_of_table = (header->num_keys == 0);
}

void btree_cursor_advance(BTreeCursor* cursor) {
    if (cursor->end_of_table) {
        return;
    }
    
    void* node = get_node(cursor->btree, cursor->page_num);
    NodeHeader* header = get_node_header(node);
    
    cursor->cell_num++;
    
    if (cursor->cell_num >= header->num_keys) {
        // TODO: Move to next leaf node
        cursor->end_of_table = true;
    }
}

bool btree_cursor_at_end(BTreeCursor* cursor) {
    return cursor->end_of_table;
}

uint32_t btree_cursor_key(BTreeCursor* cursor) {
    void* node = get_node(cursor->btree, cursor->page_num);
    uint32_t* keys = get_node_keys(node);
    return keys[cursor->cell_num];
}

RowId btree_cursor_value(BTreeCursor* cursor) {
    void* node = get_node(cursor->btree, cursor->page_num);
    RowId* values = get_leaf_node_values(node);
    return values[cursor->cell_num];
}