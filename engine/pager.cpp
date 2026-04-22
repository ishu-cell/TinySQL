/*
 * pager.cpp — LRU Buffer Pool implementation
 *
 * Manages a disk-backed page cache with LRU eviction.
 * At most BUFFER_POOL_SIZE pages live in memory at once.
 */

#include "pager.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>

Pager::Pager(const char* filename)
    : lru_head_(nullptr), lru_tail_(nullptr)
{
    // Try to open existing file for read+write (binary mode)
    file_ = std::fopen(filename, "r+b");
    if (!file_) {
        // File doesn't exist — create it
        file_ = std::fopen(filename, "w+b");
    }
    if (!file_) {
        std::cerr << "Error: Unable to open database file: " << filename << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Determine file length
    std::fseek(file_, 0, SEEK_END);
    file_length_ = static_cast<uint32_t>(std::ftell(file_));
    num_pages_   = file_length_ / PAGE_SIZE;

    if (file_length_ % PAGE_SIZE != 0) {
        std::cerr << "Warning: DB file is not aligned to page size. Possible corruption." << std::endl;
        num_pages_++; // round up
    }
}

Pager::~Pager() {
    flush_all();

    // Free all frames in the pool
    for (auto& pair : pool_) {
        delete pair.second;
    }
    pool_.clear();
    lru_head_ = nullptr;
    lru_tail_ = nullptr;

    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

// ============================================================
//  LRU Linked List Operations
// ============================================================

void Pager::lru_remove(Frame* f) {
    if (f->lru_prev) f->lru_prev->lru_next = f->lru_next;
    else             lru_head_ = f->lru_next;

    if (f->lru_next) f->lru_next->lru_prev = f->lru_prev;
    else             lru_tail_ = f->lru_prev;

    f->lru_prev = nullptr;
    f->lru_next = nullptr;
}

void Pager::lru_push_front(Frame* f) {
    f->lru_prev = nullptr;
    f->lru_next = lru_head_;

    if (lru_head_) lru_head_->lru_prev = f;
    lru_head_ = f;

    if (!lru_tail_) lru_tail_ = f;
}

void Pager::lru_promote(Frame* f) {
    if (f == lru_head_) return; // already MRU
    lru_remove(f);
    lru_push_front(f);
}

void Pager::evict_one() {
    if (!lru_tail_) return;

    Frame* victim = lru_tail_;

    // Flush if dirty
    if (victim->dirty) {
        std::fseek(file_, static_cast<long>(victim->page_num) * PAGE_SIZE, SEEK_SET);
        std::fwrite(victim->data, 1, PAGE_SIZE, file_);
        std::fflush(file_);
        victim->dirty = false;
    }

    // Remove from LRU list and pool map
    lru_remove(victim);
    pool_.erase(victim->page_num);
    delete victim;
}

// ============================================================
//  Core Page Operations
// ============================================================

void* Pager::get_page(uint32_t page_num) {
    if (page_num >= TABLE_MAX_PAGES) {
        std::cerr << "Error: Page number out of bounds (" << page_num
                  << " >= " << TABLE_MAX_PAGES << ")" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Check if already in pool
    auto it = pool_.find(page_num);
    if (it != pool_.end()) {
        lru_promote(it->second);
        return it->second->data;
    }

    // Cache miss — evict if pool is full
    while (pool_.size() >= BUFFER_POOL_SIZE) {
        evict_one();
    }

    // Allocate a new frame
    Frame* frame = new Frame();
    frame->page_num = page_num;
    frame->dirty    = false;
    frame->lru_prev = nullptr;
    frame->lru_next = nullptr;
    std::memset(frame->data, 0, PAGE_SIZE);

    // If the page already exists on disk, read it in
    if (page_num < num_pages_) {
        std::fseek(file_, static_cast<long>(page_num) * PAGE_SIZE, SEEK_SET);
        size_t bytes_read = std::fread(frame->data, 1, PAGE_SIZE, file_);
        (void)bytes_read; // partial reads are padded with zeroes
    }

    // Insert into pool and LRU list
    pool_[page_num] = frame;
    lru_push_front(frame);

    // Extend tracked page count if we jumped ahead
    if (page_num >= num_pages_) {
        num_pages_ = page_num + 1;
    }

    return frame->data;
}

void Pager::mark_dirty(uint32_t page_num) {
    auto it = pool_.find(page_num);
    if (it != pool_.end()) {
        it->second->dirty = true;
    }
}

void Pager::flush(uint32_t page_num) {
    auto it = pool_.find(page_num);
    if (it == pool_.end()) return;

    Frame* f = it->second;
    std::fseek(file_, static_cast<long>(page_num) * PAGE_SIZE, SEEK_SET);
    size_t written = std::fwrite(f->data, 1, PAGE_SIZE, file_);
    if (written < PAGE_SIZE) {
        std::cerr << "Error: Failed to write page " << page_num << " to disk." << std::endl;
    }
    f->dirty = false;
}

void Pager::flush_all() {
    for (auto& pair : pool_) {
        Frame* f = pair.second;
        if (f->dirty) {
            std::fseek(file_, static_cast<long>(f->page_num) * PAGE_SIZE, SEEK_SET);
            std::fwrite(f->data, 1, PAGE_SIZE, file_);
            f->dirty = false;
        }
    }
    if (file_) std::fflush(file_);
}

uint32_t Pager::get_unused_page_num() {
    return num_pages_;
}

uint64_t Pager::file_size() const {
    if (!file_) return 0;
    long cur = std::ftell(file_);
    std::fseek(file_, 0, SEEK_END);
    long size = std::ftell(file_);
    std::fseek(file_, cur, SEEK_SET);
    return static_cast<uint64_t>(size);
}
