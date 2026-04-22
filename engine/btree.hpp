#pragma once
/*
 * btree.hpp — B+ Tree index structure
 *
 * Each B+ Tree node occupies exactly one 4 KB page.
 *
 * Leaf nodes:   store key → Row value pairs and a next-leaf pointer
 *               forming a linked list for sequential scans.
 * Internal nodes: store keys and child page pointers for O(log n)
 *                 search routing.
 *
 * Convention:
 *   key[i] = max key in child[i]'s subtree.
 *   right_child contains keys greater than every stored key.
 */

#include "constants.hpp"
#include "pager.hpp"
#include "row.hpp"
#include <cstdint>
#include <cstring>

// ============================================================
//  Cursor — points to a specific cell in a specific leaf page
// ============================================================
struct Cursor {
    Pager*   pager;
    uint32_t page_num;
    uint32_t cell_num;
    bool     end_of_table;
};

// ============================================================
//  Common Node Accessors
// ============================================================

inline NodeType get_node_type(void* node) {
    uint8_t v = *reinterpret_cast<uint8_t*>(static_cast<char*>(node) + NODE_TYPE_OFFSET);
    return static_cast<NodeType>(v);
}

inline void set_node_type(void* node, NodeType type) {
    *reinterpret_cast<uint8_t*>(static_cast<char*>(node) + NODE_TYPE_OFFSET) = static_cast<uint8_t>(type);
}

inline bool is_node_root(void* node) {
    return *reinterpret_cast<uint8_t*>(static_cast<char*>(node) + IS_ROOT_OFFSET) != 0;
}

inline void set_node_root(void* node, bool is_root) {
    *reinterpret_cast<uint8_t*>(static_cast<char*>(node) + IS_ROOT_OFFSET) = is_root ? 1 : 0;
}

inline uint32_t* node_parent(void* node) {
    return reinterpret_cast<uint32_t*>(static_cast<char*>(node) + PARENT_POINTER_OFFSET);
}

// ============================================================
//  Leaf Node Accessors
// ============================================================

inline uint32_t* leaf_node_num_cells(void* node) {
    return reinterpret_cast<uint32_t*>(static_cast<char*>(node) + LEAF_NODE_NUM_CELLS_OFFSET);
}

inline uint32_t* leaf_node_next_leaf(void* node) {
    return reinterpret_cast<uint32_t*>(static_cast<char*>(node) + LEAF_NODE_NEXT_LEAF_OFFSET);
}

inline void* leaf_node_cell(void* node, uint32_t cell_num) {
    return static_cast<char*>(node) + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

inline uint32_t* leaf_node_key(void* node, uint32_t cell_num) {
    return reinterpret_cast<uint32_t*>(leaf_node_cell(node, cell_num));
}

inline void* leaf_node_value(void* node, uint32_t cell_num) {
    return static_cast<char*>(leaf_node_cell(node, cell_num)) + LEAF_NODE_KEY_SIZE;
}

// ============================================================
//  Internal Node Accessors
// ============================================================

inline uint32_t* internal_node_num_keys(void* node) {
    return reinterpret_cast<uint32_t*>(static_cast<char*>(node) + INTERNAL_NODE_NUM_KEYS_OFFSET);
}

inline uint32_t* internal_node_right_child(void* node) {
    return reinterpret_cast<uint32_t*>(static_cast<char*>(node) + INTERNAL_NODE_RIGHT_CHILD_OFFSET);
}

inline void* internal_node_cell(void* node, uint32_t cell_num) {
    return static_cast<char*>(node) + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
}

inline uint32_t* internal_node_child(void* node, uint32_t cell_num) {
    return reinterpret_cast<uint32_t*>(internal_node_cell(node, cell_num));
}

inline uint32_t* internal_node_key(void* node, uint32_t key_num) {
    return reinterpret_cast<uint32_t*>(
        static_cast<char*>(internal_node_cell(node, key_num)) + INTERNAL_NODE_CHILD_SIZE);
}

// ============================================================
//  Initialization
// ============================================================

inline void initialize_leaf_node(void* node) {
    std::memset(node, 0, PAGE_SIZE);
    set_node_type(node, NodeType::LEAF);
    set_node_root(node, false);
    *leaf_node_num_cells(node)  = 0;
    *leaf_node_next_leaf(node)  = 0; // 0 = no next leaf
}

inline void initialize_internal_node(void* node) {
    std::memset(node, 0, PAGE_SIZE);
    set_node_type(node, NodeType::INTERNAL);
    set_node_root(node, false);
    *internal_node_num_keys(node) = 0;
}

// ============================================================
//  Non-inline function declarations (implemented in btree.cpp)
// ============================================================

/// Recursively find the max key in the subtree rooted at `node`.
uint32_t get_node_max_key(Pager& pager, void* node);

/// Binary-search the tree for `key`.  Returns a Cursor pointing to
/// the leaf cell where `key` IS or WOULD BE inserted.
Cursor btree_find(Pager& pager, uint32_t root_page_num, uint32_t key);

/// Return a cursor positioned at the very first row (leftmost leaf, cell 0).
Cursor table_start(Pager& pager, uint32_t root_page_num);

/// Advance the cursor to the next cell, following next-leaf links.
void cursor_advance(Cursor& cursor);

/// Insert a row at the position indicated by `cursor`.
/// Handles node splitting automatically (including internal node splits).
void leaf_node_insert(Cursor& cursor, uint32_t key, const Row& value);

/// Delete a key from the B+ Tree (lazy — removes cell and shifts).
/// Returns true if key was found and deleted, false otherwise.
bool btree_delete(Pager& pager, uint32_t root_page_num, uint32_t key);

/// Count all rows in the B+ Tree by traversing the leaf linked list.
uint32_t btree_count(Pager& pager, uint32_t root_page_num);

/// Debug: print the B+ Tree structure to stdout.
void print_tree(Pager& pager, uint32_t page_num, uint32_t indent);
