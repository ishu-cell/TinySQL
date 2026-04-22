/*
 * btree.cpp — B+ Tree operations
 *
 * Implements search, insert, leaf/internal node splitting (full support),
 * lazy deletion, counting, root promotion, and sequential scan via cursors.
 */

#include "btree.hpp"
#include <iostream>
#include <cstring>

// ============================================================
//  Helpers
// ============================================================

/// Recursively find the max key in the subtree rooted at `node`.
uint32_t get_node_max_key(Pager& pager, void* node) {
    if (get_node_type(node) == NodeType::LEAF) {
        uint32_t n = *leaf_node_num_cells(node);
        if (n == 0) return 0;
        return *leaf_node_key(node, n - 1);
    }
    // Internal: recurse into the rightmost child
    void* right = pager.get_page(*internal_node_right_child(node));
    return get_node_max_key(pager, right);
}

/// Find which index in an internal node a given child page lives at.
/// Returns num_keys if the child is the right_child.
static uint32_t internal_node_find_child_index(void* node, uint32_t child_page_num) {
    uint32_t num_keys = *internal_node_num_keys(node);
    for (uint32_t i = 0; i < num_keys; i++) {
        if (*internal_node_child(node, i) == child_page_num) {
            return i;
        }
    }
    return num_keys; // it's the right_child
}

// ============================================================
//  Search
// ============================================================

/// Binary-search a leaf node for `key`.
static Cursor leaf_node_find(Pager& pager, uint32_t page_num, uint32_t key) {
    void* node = pager.get_page(page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    Cursor cursor{};
    cursor.pager        = &pager;
    cursor.page_num     = page_num;
    cursor.end_of_table = false;

    // Binary search for the position
    uint32_t lo = 0, hi = num_cells;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        uint32_t k   = *leaf_node_key(node, mid);
        if (key == k) { cursor.cell_num = mid; return cursor; }
        if (key < k)  hi = mid;
        else          lo = mid + 1;
    }
    cursor.cell_num = lo;
    return cursor;
}

/// Binary-search an internal node to find which child to recurse into.
static Cursor internal_node_find(Pager& pager, uint32_t page_num, uint32_t key) {
    void* node     = pager.get_page(page_num);
    uint32_t nkeys = *internal_node_num_keys(node);

    uint32_t lo = 0, hi = nkeys;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        uint32_t k   = *internal_node_key(node, mid);
        if (key <= k) hi = mid;
        else          lo = mid + 1;
    }

    uint32_t child_page;
    if (lo >= nkeys) child_page = *internal_node_right_child(node);
    else             child_page = *internal_node_child(node, lo);

    return btree_find(pager, child_page, key);
}

Cursor btree_find(Pager& pager, uint32_t root_page_num, uint32_t key) {
    void* root = pager.get_page(root_page_num);
    if (get_node_type(root) == NodeType::LEAF)
        return leaf_node_find(pager, root_page_num, key);
    return internal_node_find(pager, root_page_num, key);
}

// ============================================================
//  Cursor helpers (for sequential scan / select)
// ============================================================

Cursor table_start(Pager& pager, uint32_t root_page_num) {
    // Walk to the leftmost leaf
    uint32_t page_num = root_page_num;
    void* node = pager.get_page(page_num);

    while (get_node_type(node) == NodeType::INTERNAL) {
        page_num = *internal_node_child(node, 0);
        node     = pager.get_page(page_num);
    }

    Cursor cursor{};
    cursor.pager        = &pager;
    cursor.page_num     = page_num;
    cursor.cell_num     = 0;
    cursor.end_of_table = (*leaf_node_num_cells(node) == 0);
    return cursor;
}

void cursor_advance(Cursor& cursor) {
    void* node = cursor.pager->get_page(cursor.page_num);
    cursor.cell_num++;

    if (cursor.cell_num >= *leaf_node_num_cells(node)) {
        uint32_t next = *leaf_node_next_leaf(node);
        if (next == 0) {
            // No more leaves — end of table
            cursor.end_of_table = true;
        } else {
            cursor.page_num = next;
            cursor.cell_num = 0;
        }
    }
}

// ============================================================
//  Forward declarations (internal split helpers)
// ============================================================
static void leaf_node_split_and_insert(Cursor& cursor, uint32_t key, const Row& value);
static void create_new_root(Pager& pager, uint32_t right_child_page_num);
static void internal_node_insert(Pager& pager, uint32_t parent_page_num, uint32_t child_page_num);
static void internal_node_split_and_insert(Pager& pager, uint32_t parent_page_num, uint32_t child_page_num);

// ============================================================
//  Insert
// ============================================================

void leaf_node_insert(Cursor& cursor, uint32_t key, const Row& value) {
    void* node = cursor.pager->get_page(cursor.page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    if (num_cells >= LEAF_NODE_MAX_CELLS) {
        // Node is full — must split
        leaf_node_split_and_insert(cursor, key, value);
        return;
    }

    // Shift cells to the right to make room at cursor.cell_num
    if (cursor.cell_num < num_cells) {
        for (uint32_t i = num_cells; i > cursor.cell_num; i--) {
            std::memcpy(leaf_node_cell(node, i),
                        leaf_node_cell(node, i - 1),
                        LEAF_NODE_CELL_SIZE);
        }
    }

    // Write the new cell
    *leaf_node_key(node, cursor.cell_num) = key;
    serialize_row(value, leaf_node_value(node, cursor.cell_num));
    *leaf_node_num_cells(node) = num_cells + 1;

    cursor.pager->mark_dirty(cursor.page_num);
}

// ============================================================
//  Leaf Node Splitting
// ============================================================

static void leaf_node_split_and_insert(Cursor& cursor, uint32_t key, const Row& value) {
    void* old_node     = cursor.pager->get_page(cursor.page_num);
    uint32_t old_max   = get_node_max_key(*cursor.pager, old_node);
    (void)old_max;

    uint32_t new_page_num = cursor.pager->get_unused_page_num();
    void* new_node        = cursor.pager->get_page(new_page_num);
    initialize_leaf_node(new_node);
    set_node_root(new_node, false);
    *node_parent(new_node) = *node_parent(old_node);

    // Maintain the leaf linked list:  old → new → old_next
    *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
    *leaf_node_next_leaf(old_node) = new_page_num;

    /*
     * Redistribute.  We iterate over the virtual array of
     * LEAF_NODE_MAX_CELLS + 1 entries (old cells + new cell)
     * from right to left, placing each into either the old
     * (left) or new (right) node.
     */
    for (int32_t i = static_cast<int32_t>(LEAF_NODE_MAX_CELLS); i >= 0; i--) {
        void*    dest_node;
        uint32_t dest_index;

        if (static_cast<uint32_t>(i) >= LEAF_NODE_LEFT_SPLIT_COUNT) {
            dest_node  = new_node;
            dest_index = static_cast<uint32_t>(i) - LEAF_NODE_LEFT_SPLIT_COUNT;
        } else {
            dest_node  = old_node;
            dest_index = static_cast<uint32_t>(i);
        }

        if (static_cast<uint32_t>(i) == cursor.cell_num) {
            // This is the new cell
            serialize_row(value, leaf_node_value(dest_node, dest_index));
            *leaf_node_key(dest_node, dest_index) = key;
        } else if (static_cast<uint32_t>(i) > cursor.cell_num) {
            // Shifted from old position i-1
            std::memcpy(leaf_node_cell(dest_node, dest_index),
                        leaf_node_cell(old_node, i - 1),
                        LEAF_NODE_CELL_SIZE);
        } else {
            // Unchanged position
            std::memcpy(leaf_node_cell(dest_node, dest_index),
                        leaf_node_cell(old_node, static_cast<uint32_t>(i)),
                        LEAF_NODE_CELL_SIZE);
        }
    }

    // Update cell counts
    *leaf_node_num_cells(old_node) = LEAF_NODE_LEFT_SPLIT_COUNT;
    *leaf_node_num_cells(new_node) = LEAF_NODE_RIGHT_SPLIT_COUNT;

    cursor.pager->mark_dirty(cursor.page_num);
    cursor.pager->mark_dirty(new_page_num);

    if (is_node_root(old_node)) {
        // The root leaf just split — promote to a new internal root
        create_new_root(*cursor.pager, new_page_num);
    } else {
        // Update the parent: fix old child's key, then insert new child
        uint32_t parent_page_num = *node_parent(old_node);
        void* parent = cursor.pager->get_page(parent_page_num);

        uint32_t old_child_idx = internal_node_find_child_index(parent, cursor.page_num);
        // If old child is a regular child (not right_child), update its key
        if (old_child_idx < *internal_node_num_keys(parent)) {
            *internal_node_key(parent, old_child_idx) =
                get_node_max_key(*cursor.pager, old_node);
            cursor.pager->mark_dirty(parent_page_num);
        }

        internal_node_insert(*cursor.pager, parent_page_num, new_page_num);
    }
}

// ============================================================
//  Root Promotion
// ============================================================

static void create_new_root(Pager& pager, uint32_t right_child_page_num) {
    void* root        = pager.get_page(0);
    void* right_child = pager.get_page(right_child_page_num);

    // Allocate a new page for the left child (copy of old root)
    uint32_t left_child_page_num = pager.get_unused_page_num();
    void* left_child = pager.get_page(left_child_page_num);

    // Copy existing root data → left child
    std::memcpy(left_child, root, PAGE_SIZE);
    set_node_root(left_child, false);
    *node_parent(left_child) = 0;

    // If the left child is internal, update its children's parent pointers
    if (get_node_type(left_child) == NodeType::INTERNAL) {
        uint32_t nk = *internal_node_num_keys(left_child);
        for (uint32_t i = 0; i < nk; i++) {
            void* child = pager.get_page(*internal_node_child(left_child, i));
            *node_parent(child) = left_child_page_num;
            pager.mark_dirty(*internal_node_child(left_child, i));
        }
        void* rc = pager.get_page(*internal_node_right_child(left_child));
        *node_parent(rc) = left_child_page_num;
        pager.mark_dirty(*internal_node_right_child(left_child));
    }

    // Reinitialize page 0 as a fresh internal root
    initialize_internal_node(root);
    set_node_root(root, true);

    *internal_node_num_keys(root) = 1;
    *internal_node_child(root, 0)  = left_child_page_num;
    *internal_node_key(root, 0)    = get_node_max_key(pager, left_child);
    *internal_node_right_child(root) = right_child_page_num;

    *node_parent(left_child)  = 0;
    *node_parent(right_child) = 0;

    pager.mark_dirty(0);
    pager.mark_dirty(left_child_page_num);
    pager.mark_dirty(right_child_page_num);
}

// ============================================================
//  Internal Node Insert (with splitting support)
// ============================================================

/*
 * Add a new child pointer to an internal node after a child split.
 * If the internal node is full, split it first.
 */
static void internal_node_insert(Pager& pager, uint32_t parent_page_num, uint32_t child_page_num) {
    void* parent = pager.get_page(parent_page_num);
    void* child  = pager.get_page(child_page_num);

    uint32_t child_max_key = get_node_max_key(pager, child);
    uint32_t num_keys      = *internal_node_num_keys(parent);

    if (num_keys >= INTERNAL_NODE_MAX_KEYS) {
        // Internal node full — split it, then insert into the correct half
        internal_node_split_and_insert(pager, parent_page_num, child_page_num);
        return;
    }

    uint32_t right_child_page = *internal_node_right_child(parent);
    void* right_child_node    = pager.get_page(right_child_page);
    uint32_t right_max        = get_node_max_key(pager, right_child_node);

    if (child_max_key > right_max) {
        // New child has the largest keys — it becomes the right_child.
        // Demote old right_child to a regular cell.
        *internal_node_child(parent, num_keys) = right_child_page;
        *internal_node_key(parent, num_keys)   = right_max;
        *internal_node_right_child(parent)     = child_page_num;
    } else {
        // Find sorted position
        uint32_t index = num_keys;
        for (uint32_t i = 0; i < num_keys; i++) {
            if (child_max_key <= *internal_node_key(parent, i)) {
                index = i;
                break;
            }
        }
        // Shift cells right to make room
        for (uint32_t i = num_keys; i > index; i--) {
            std::memcpy(internal_node_cell(parent, i),
                        internal_node_cell(parent, i - 1),
                        INTERNAL_NODE_CELL_SIZE);
        }
        *internal_node_child(parent, index) = child_page_num;
        *internal_node_key(parent, index)   = child_max_key;
    }

    *internal_node_num_keys(parent) = num_keys + 1;
    *node_parent(child) = parent_page_num;

    pager.mark_dirty(parent_page_num);
}

// ============================================================
//  Internal Node Splitting
// ============================================================

/*
 * Split a full internal node and insert a new child.
 *
 * 1. Allocate a new (right) internal page.
 * 2. Move the upper half of keys/children to the new node.
 * 3. The median key is promoted to the parent (or a new root).
 * 4. Insert the new child into the appropriate half.
 */
static void internal_node_split_and_insert(Pager& pager, uint32_t parent_page_num, uint32_t child_page_num) {
    void* old_node = pager.get_page(parent_page_num);
    uint32_t old_num_keys = *internal_node_num_keys(old_node);

    // Allocate new internal node
    uint32_t new_page_num = pager.get_unused_page_num();
    void* new_node = pager.get_page(new_page_num);
    initialize_internal_node(new_node);
    set_node_root(new_node, false);

    // Split point: left gets [0..mid-1], median key promoted, right gets [mid+1..end]
    uint32_t mid = old_num_keys / 2;

    // Copy right half (mid+1 .. old_num_keys-1) to new node
    uint32_t new_key_count = 0;
    for (uint32_t i = mid + 1; i < old_num_keys; i++) {
        *internal_node_child(new_node, new_key_count) = *internal_node_child(old_node, i);
        *internal_node_key(new_node, new_key_count)   = *internal_node_key(old_node, i);

        // Update child's parent pointer
        void* c = pager.get_page(*internal_node_child(old_node, i));
        *node_parent(c) = new_page_num;
        pager.mark_dirty(*internal_node_child(old_node, i));

        new_key_count++;
    }

    // The old right_child becomes new node's right_child
    *internal_node_right_child(new_node) = *internal_node_right_child(old_node);
    void* old_rc = pager.get_page(*internal_node_right_child(old_node));
    *node_parent(old_rc) = new_page_num;
    pager.mark_dirty(*internal_node_right_child(old_node));

    *internal_node_num_keys(new_node) = new_key_count;

    // The child at position [mid] becomes old node's right_child (its key is promoted)
    *internal_node_right_child(old_node) = *internal_node_child(old_node, mid);
    // The key at [mid] will be promoted to the parent
    // uint32_t promoted_key = *internal_node_key(old_node, mid);  -- used below

    *internal_node_num_keys(old_node) = mid;

    pager.mark_dirty(parent_page_num);
    pager.mark_dirty(new_page_num);

    // Now insert the new child into the appropriate half
    void* child = pager.get_page(child_page_num);
    uint32_t child_max = get_node_max_key(pager, child);
    uint32_t old_max   = get_node_max_key(pager, pager.get_page(parent_page_num));

    if (child_max <= old_max) {
        internal_node_insert(pager, parent_page_num, child_page_num);
    } else {
        internal_node_insert(pager, new_page_num, child_page_num);
    }

    // Set parent pointer for new_node
    *node_parent(new_node) = *node_parent(old_node);

    // Promote: if old node was root, create new root
    if (is_node_root(old_node)) {
        create_new_root(pager, new_page_num);
    } else {
        uint32_t grandparent_page = *node_parent(old_node);
        void* grandparent = pager.get_page(grandparent_page);

        // Update old node's key in grandparent
        uint32_t idx = internal_node_find_child_index(grandparent, parent_page_num);
        if (idx < *internal_node_num_keys(grandparent)) {
            *internal_node_key(grandparent, idx) = get_node_max_key(pager, pager.get_page(parent_page_num));
            pager.mark_dirty(grandparent_page);
        }

        internal_node_insert(pager, grandparent_page, new_page_num);
    }
}

// ============================================================
//  Lazy Deletion
// ============================================================

bool btree_delete(Pager& pager, uint32_t root_page_num, uint32_t key) {
    Cursor cursor = btree_find(pager, root_page_num, key);
    void* node = pager.get_page(cursor.page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    // Check if the key actually exists at this position
    if (cursor.cell_num >= num_cells ||
        *leaf_node_key(node, cursor.cell_num) != key) {
        return false; // key not found
    }

    // Shift cells left to fill the gap (lazy — no rebalancing)
    for (uint32_t i = cursor.cell_num; i < num_cells - 1; i++) {
        std::memcpy(leaf_node_cell(node, i),
                    leaf_node_cell(node, i + 1),
                    LEAF_NODE_CELL_SIZE);
    }
    *leaf_node_num_cells(node) = num_cells - 1;

    pager.mark_dirty(cursor.page_num);
    return true;
}

// ============================================================
//  Count
// ============================================================

uint32_t btree_count(Pager& pager, uint32_t root_page_num) {
    uint32_t count = 0;
    Cursor cursor = table_start(pager, root_page_num);
    while (!cursor.end_of_table) {
        count++;
        cursor_advance(cursor);
    }
    return count;
}

// ============================================================
//  Debug: Print Tree
// ============================================================

void print_tree(Pager& pager, uint32_t page_num, uint32_t indent) {
    void* node = pager.get_page(page_num);
    std::string pad(indent, ' ');

    switch (get_node_type(node)) {
    case NodeType::LEAF: {
        uint32_t nc = *leaf_node_num_cells(node);
        std::cout << pad << "- leaf (size " << nc << ")\n";
        for (uint32_t i = 0; i < nc; i++) {
            std::cout << pad << "  - " << *leaf_node_key(node, i) << "\n";
        }
        break;
    }
    case NodeType::INTERNAL: {
        uint32_t nk = *internal_node_num_keys(node);
        std::cout << pad << "- internal (size " << nk << ")\n";
        for (uint32_t i = 0; i < nk; i++) {
            print_tree(pager, *internal_node_child(node, i), indent + 2);
            std::cout << pad << "  - key " << *internal_node_key(node, i) << "\n";
        }
        print_tree(pager, *internal_node_right_child(node), indent + 2);
        break;
    }
    }
}
