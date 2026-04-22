#pragma once
/*
 * constants.hpp — TinySQL core constants and memory layout definitions
 *
 * Defines page sizes, row layout offsets, B+ Tree node formats,
 * buffer pool limits, and heap file constants.
 * All sizes are in bytes. Offsets are relative to the start of a page.
 */

#include <cstdint>

// ============================================================
//  Page Configuration
// ============================================================
const uint32_t PAGE_SIZE          = 4096;
const uint32_t TABLE_MAX_PAGES    = 1000000;  // max pages on disk
const uint32_t BUFFER_POOL_SIZE   = 1024;     // max pages in memory (LRU)

// ============================================================
//  Row Layout  (id: 4B | username: 32B | email: 255B = 291B)
// ============================================================
const uint32_t COLUMN_ID_SIZE       = sizeof(uint32_t);   // 4
const uint32_t COLUMN_USERNAME_SIZE = 32;
const uint32_t COLUMN_EMAIL_SIZE    = 255;
const uint32_t ROW_SIZE = COLUMN_ID_SIZE + COLUMN_USERNAME_SIZE + COLUMN_EMAIL_SIZE; // 291

const uint32_t ID_OFFSET       = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + COLUMN_ID_SIZE;          // 4
const uint32_t EMAIL_OFFSET    = USERNAME_OFFSET + COLUMN_USERNAME_SIZE; // 36

// ============================================================
//  Node Types
// ============================================================
enum class NodeType : uint8_t { INTERNAL = 0, LEAF = 1 };

// ============================================================
//  Common Node Header  (type: 1B | is_root: 1B | parent_ptr: 4B = 6B)
// ============================================================
const uint32_t NODE_TYPE_SIZE          = sizeof(uint8_t);     // 1
const uint32_t NODE_TYPE_OFFSET        = 0;
const uint32_t IS_ROOT_SIZE            = sizeof(uint8_t);     // 1
const uint32_t IS_ROOT_OFFSET          = NODE_TYPE_SIZE;      // 1
const uint32_t PARENT_POINTER_SIZE     = sizeof(uint32_t);    // 4
const uint32_t PARENT_POINTER_OFFSET   = IS_ROOT_OFFSET + IS_ROOT_SIZE; // 2
const uint32_t COMMON_NODE_HEADER_SIZE = NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE; // 6

// ============================================================
//  Leaf Node Layout
//  Header: common(6B) + num_cells(4B) + next_leaf(4B) = 14B
//  Cell:   key(4B) + value(291B) = 295B
// ============================================================
const uint32_t LEAF_NODE_NUM_CELLS_SIZE   = sizeof(uint32_t);       // 4
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE; // 6
const uint32_t LEAF_NODE_NEXT_LEAF_SIZE   = sizeof(uint32_t);       // 4
const uint32_t LEAF_NODE_NEXT_LEAF_OFFSET = LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE; // 10
const uint32_t LEAF_NODE_HEADER_SIZE      = COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE; // 14

const uint32_t LEAF_NODE_KEY_SIZE         = sizeof(uint32_t);  // 4
const uint32_t LEAF_NODE_KEY_OFFSET       = 0;
const uint32_t LEAF_NODE_VALUE_SIZE       = ROW_SIZE;          // 291
const uint32_t LEAF_NODE_VALUE_OFFSET     = LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE; // 4
const uint32_t LEAF_NODE_CELL_SIZE        = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;  // 295

const uint32_t LEAF_NODE_SPACE_FOR_CELLS  = PAGE_SIZE - LEAF_NODE_HEADER_SIZE; // 4082
const uint32_t LEAF_NODE_MAX_CELLS        = LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE; // 13

// Split counts (14 cells → 7 left + 7 right)
const uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2;                           // 7
const uint32_t LEAF_NODE_LEFT_SPLIT_COUNT  = (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT; // 7

// ============================================================
//  Internal Node Layout
//  Header: common(6B) + num_keys(4B) + right_child(4B) = 14B
//  Cell:   child_ptr(4B) + key(4B) = 8B
// ============================================================
const uint32_t INTERNAL_NODE_NUM_KEYS_SIZE      = sizeof(uint32_t);       // 4
const uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET    = COMMON_NODE_HEADER_SIZE; // 6
const uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE   = sizeof(uint32_t);       // 4
const uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET = INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE; // 10
const uint32_t INTERNAL_NODE_HEADER_SIZE        = COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE; // 14

const uint32_t INTERNAL_NODE_KEY_SIZE   = sizeof(uint32_t);   // 4
const uint32_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t);   // 4
const uint32_t INTERNAL_NODE_CELL_SIZE  = INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE; // 8

const uint32_t INTERNAL_NODE_MAX_KEYS = (PAGE_SIZE - INTERNAL_NODE_HEADER_SIZE) / INTERNAL_NODE_CELL_SIZE; // 510

// ============================================================
//  Heap File Layout
//  Header per page: num_rows(4B) = 4B
//  Row slot: tombstone(1B) + row(291B) = 292B
// ============================================================
const uint32_t HEAP_PAGE_HEADER_SIZE  = sizeof(uint32_t);  // 4  (num_rows stored)
const uint32_t HEAP_TOMBSTONE_SIZE    = sizeof(uint8_t);   // 1
const uint32_t HEAP_SLOT_SIZE         = HEAP_TOMBSTONE_SIZE + ROW_SIZE;  // 292
const uint32_t HEAP_SPACE_FOR_ROWS    = PAGE_SIZE - HEAP_PAGE_HEADER_SIZE; // 4092
const uint32_t HEAP_ROWS_PER_PAGE     = HEAP_SPACE_FOR_ROWS / HEAP_SLOT_SIZE; // 14 (4092/292=14)
