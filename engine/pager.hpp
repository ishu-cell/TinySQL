#pragma once
/*
 * pager.hpp — LRU Buffer Pool backed by disk
 *
 * The Pager manages a file as an array of fixed-size 4 KB pages.
 * An LRU (Least Recently Used) eviction strategy ensures at most
 * BUFFER_POOL_SIZE pages are resident in memory at any time.
 *
 * Key data structures:
 *   - std::unordered_map<page_num, Frame*> for O(1) lookup
 *   - Doubly-linked list of Frames for LRU ordering
 *     (head = most recently used, tail = least recently used)
 */

#include "constants.hpp"
#include <cstdint>
#include <cstdio>
#include <unordered_map>

struct Frame {
    uint32_t page_num;
    char     data[PAGE_SIZE];
    bool     dirty;
    Frame*   lru_prev;
    Frame*   lru_next;
};

class Pager {
public:
    explicit Pager(const char* filename);
    ~Pager();

    // Disallow copy
    Pager(const Pager&) = delete;
    Pager& operator=(const Pager&) = delete;

    /// Return a pointer to the in-memory page data.
    /// If the page is not cached, load from disk (evicting LRU if pool is full).
    void* get_page(uint32_t page_num);

    /// Mark a page as dirty so it will be flushed on eviction or flush_all().
    void mark_dirty(uint32_t page_num);

    /// Write a single page to disk (if cached).
    void flush(uint32_t page_num);

    /// Flush every dirty cached page to disk and sync.
    void flush_all();

    /// Allocate a fresh page number (appends to the end of the file).
    uint32_t get_unused_page_num();

    /// Number of pages currently on disk (file length / PAGE_SIZE).
    uint32_t num_pages() const { return num_pages_; }

    /// File size in bytes.
    uint64_t file_size() const;

    /// Number of pages currently in the buffer pool.
    uint32_t pool_occupancy() const { return static_cast<uint32_t>(pool_.size()); }

private:
    FILE*     file_;
    uint32_t  file_length_;
    uint32_t  num_pages_;

    // --- LRU Buffer Pool ---
    std::unordered_map<uint32_t, Frame*> pool_;
    Frame*    lru_head_;  // most recently used
    Frame*    lru_tail_;  // least recently used

    void lru_promote(Frame* f);
    void lru_remove(Frame* f);
    void lru_push_front(Frame* f);
    void evict_one();
};
