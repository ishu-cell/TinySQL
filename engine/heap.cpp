/*
 * heap.cpp — Heap File implementation
 *
 * Append-only storage with tombstone deletion and linear scan search.
 */

#include "heap.hpp"
#include <cstring>
#include <iostream>

// ============================================================
//  Page Layout Accessors
// ============================================================

uint32_t* HeapFile::heap_num_rows(void* page) {
    return reinterpret_cast<uint32_t*>(page);
}

uint8_t* HeapFile::heap_slot_tombstone(void* page, uint32_t slot) {
    return reinterpret_cast<uint8_t*>(
        static_cast<char*>(page) + HEAP_PAGE_HEADER_SIZE + slot * HEAP_SLOT_SIZE
    );
}

void* HeapFile::heap_slot_data(void* page, uint32_t slot) {
    return static_cast<char*>(page)
         + HEAP_PAGE_HEADER_SIZE
         + slot * HEAP_SLOT_SIZE
         + HEAP_TOMBSTONE_SIZE;
}

// ============================================================
//  Constructor / Destructor
// ============================================================

HeapFile::HeapFile(const char* filename)
    : pager_(filename)
{
    // If brand new file, initialize the first page
    if (pager_.num_pages() == 0) {
        void* page = pager_.get_page(0);
        std::memset(page, 0, PAGE_SIZE);
        *heap_num_rows(page) = 0;
        pager_.mark_dirty(0);
    }
}

HeapFile::~HeapFile() {
    pager_.flush_all();
}

// ============================================================
//  Insert — O(1) append
// ============================================================

bool HeapFile::insert(const Row& row) {
    // Find the last page
    uint32_t last_page_num = (pager_.num_pages() == 0) ? 0 : pager_.num_pages() - 1;
    void* page = pager_.get_page(last_page_num);
    uint32_t num_rows = *heap_num_rows(page);

    // If the last page is full, allocate a new one
    if (num_rows >= HEAP_ROWS_PER_PAGE) {
        last_page_num = pager_.get_unused_page_num();
        page = pager_.get_page(last_page_num);
        std::memset(page, 0, PAGE_SIZE);
        *heap_num_rows(page) = 0;
        num_rows = 0;
    }

    // Write tombstone = 0 (alive) + row data
    *heap_slot_tombstone(page, num_rows) = 0;
    serialize_row(row, heap_slot_data(page, num_rows));
    *heap_num_rows(page) = num_rows + 1;

    pager_.mark_dirty(last_page_num);
    return true;
}

// ============================================================
//  Find — O(n) linear scan
// ============================================================

bool HeapFile::find(uint32_t id, Row& out) const {
    uint32_t total_pages = pager_.num_pages();
    for (uint32_t p = 0; p < total_pages; p++) {
        void* page = pager_.get_page(p);
        uint32_t num_rows = *heap_num_rows(page);

        for (uint32_t s = 0; s < num_rows; s++) {
            // Skip tombstoned rows
            if (*heap_slot_tombstone(page, s) != 0) continue;

            Row r;
            deserialize_row(heap_slot_data(page, s), r);
            if (r.id == id) {
                out = r;
                return true;
            }
        }
    }
    return false;
}

// ============================================================
//  Delete — O(n) tombstone
// ============================================================

bool HeapFile::remove(uint32_t id) {
    uint32_t total_pages = pager_.num_pages();
    for (uint32_t p = 0; p < total_pages; p++) {
        void* page = pager_.get_page(p);
        uint32_t num_rows = *heap_num_rows(page);

        for (uint32_t s = 0; s < num_rows; s++) {
            if (*heap_slot_tombstone(page, s) != 0) continue;

            Row r;
            deserialize_row(heap_slot_data(page, s), r);
            if (r.id == id) {
                *heap_slot_tombstone(page, s) = 1; // mark as deleted
                pager_.mark_dirty(p);
                return true;
            }
        }
    }
    return false;
}

// ============================================================
//  Count — O(n) scan
// ============================================================

uint32_t HeapFile::count() const {
    uint32_t total = 0;
    uint32_t total_pages = pager_.num_pages();
    for (uint32_t p = 0; p < total_pages; p++) {
        void* page = pager_.get_page(p);
        uint32_t num_rows = *heap_num_rows(page);

        for (uint32_t s = 0; s < num_rows; s++) {
            if (*heap_slot_tombstone(page, s) == 0) {
                total++;
            }
        }
    }
    return total;
}
