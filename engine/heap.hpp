#pragma once
/*
 * heap.hpp — Heap File structure
 *
 * A simple append-only file organisation for comparison benchmarking
 * against the B+ Tree.
 *
 * Layout per page:
 *   [num_rows: 4B]
 *   [slot 0: tombstone(1B) + row(291B)] = 292B
 *   [slot 1: ...]
 *   ... up to HEAP_ROWS_PER_PAGE slots
 *
 * Insert:  O(1) — append to last page
 * Search:  O(n) — linear scan all pages / slots
 * Delete:  tombstone flag (lazy)
 */

#include "constants.hpp"
#include "pager.hpp"
#include "row.hpp"
#include <cstdint>

class HeapFile {
public:
    explicit HeapFile(const char* filename);
    ~HeapFile();

    // Disallow copy
    HeapFile(const HeapFile&) = delete;
    HeapFile& operator=(const HeapFile&) = delete;

    /// O(1) append-only insert. Returns true on success.
    bool insert(const Row& row);

    /// O(n) linear scan to find a row by ID.
    /// Returns true if found, writing into `out`.
    bool find(uint32_t id, Row& out) const;

    /// O(n) tombstone deletion. Returns true if key was found and deleted.
    bool remove(uint32_t id);

    /// Count all live (non-tombstoned) rows.
    uint32_t count() const;

    /// File size in bytes.
    uint64_t file_size() const { return pager_.file_size(); }

    /// Access the underlying pager (for flush).
    Pager& pager() { return pager_; }

private:
    mutable Pager pager_;

    // Accessors for heap page layout
    static uint32_t* heap_num_rows(void* page);
    static uint8_t*  heap_slot_tombstone(void* page, uint32_t slot);
    static void*     heap_slot_data(void* page, uint32_t slot);
};
