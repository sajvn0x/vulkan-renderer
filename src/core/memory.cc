#include "memory.hh"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>

namespace Memory {

namespace {

struct alignas(max_align_t) AllocationHeader {
    size_t size;
};

std::mutex stats_mutex;
Statistics stats;

void record_allocation(size_t p_size) {
    std::lock_guard lock(stats_mutex);

    stats.current_bytes += p_size;
    stats.live_allocations++;
    stats.total_allocations++;

    if (stats.current_bytes > stats.peak_bytes) {
        stats.peak_bytes = stats.current_bytes;
    }
}

void record_free(size_t p_size) {
    std::lock_guard lock(stats_mutex);

    stats.current_bytes -= p_size;
    stats.live_allocations--;
    stats.total_frees++;
}

void record_failure() {
    std::lock_guard lock(stats_mutex);
    stats.failed_allocations++;
}

AllocationHeader *header_from(void *p_ptr) { return static_cast<AllocationHeader *>(p_ptr) - 1; }

};  // namespace

Statistics get_statistics() {
    std::lock_guard lock(stats_mutex);
    return stats;
}

void print_statistics() {
    const Statistics s = get_statistics();

    printf(
        "Memory statistics:\n"
        "  Current bytes:       %llu\n"
        "  Peak bytes:          %llu\n"
        "  Live allocations:    %llu\n"
        "  Total allocations:   %llu\n"
        "  Total frees:         %llu\n"
        "  Failed allocations:  %llu\n",
        static_cast<unsigned long long>(s.current_bytes),
        static_cast<unsigned long long>(s.peak_bytes),
        static_cast<unsigned long long>(s.live_allocations),
        static_cast<unsigned long long>(s.total_allocations),
        static_cast<unsigned long long>(s.total_frees),
        static_cast<unsigned long long>(s.failed_allocations));
}

};  // namespace Memory

void *mem_alloc(size_t p_size) {
    if (p_size == 0) {
        p_size = 1;
    }

    if (p_size > std::numeric_limits<size_t>::max() - sizeof(Memory::AllocationHeader)) {
        Memory::record_failure();
        return nullptr;
    }

    void *raw = malloc(sizeof(Memory::AllocationHeader) + p_size);

    if (!raw) {
        Memory::record_failure();
        return nullptr;
    }

    Memory::AllocationHeader *header = static_cast<Memory::AllocationHeader *>(raw);
    header->size = p_size;

    Memory::record_allocation(p_size);
    return header + 1;
}

void *mem_calloc(size_t p_count, size_t p_size) {
    if (p_count != 0 && p_size > std::numeric_limits<size_t>::max() / p_count) {
        Memory::record_failure();
        return nullptr;
    }

    const size_t total = p_count * p_size;
    void *ptr = mem_alloc(total);

    if (ptr && total != 0) {
        memset(ptr, 0, total);
    }

    return ptr;
}

void *mem_realloc(void *p_ptr, size_t p_new_size) {
    if (!p_ptr) {
        return mem_alloc(p_new_size);
    }

    if (p_new_size == 0) {
        mem_free(p_ptr);
        return nullptr;
    }

    Memory::AllocationHeader *old_header = Memory::header_from(p_ptr);
    const size_t old_size = old_header->size;

    if (p_new_size > std::numeric_limits<size_t>::max() - sizeof(Memory::AllocationHeader)) {
        Memory::record_failure();
        return nullptr;
    }

    void *raw = realloc(old_header, sizeof(Memory::AllocationHeader) + p_new_size);

    if (!raw) {
        Memory::record_failure();
        return nullptr;
    }

    Memory::AllocationHeader *new_header = Memory::header_from(raw);
    new_header->size = p_new_size;

    {
        std::lock_guard lock(Memory::stats_mutex);

        if (p_new_size >= old_size) {
            Memory::stats.current_bytes += p_new_size - old_size;
        } else {
            Memory::stats.current_bytes -= old_size - p_new_size;
        }

        Memory::stats.total_allocations++;

        if (Memory::stats.current_bytes > Memory::stats.peak_bytes) {
            Memory::stats.peak_bytes = Memory::stats.current_bytes;
        }
    }

    return new_header + 1;
}

void mem_free(void *p_ptr) {
    if (!p_ptr) {
        return;
    }

    Memory::AllocationHeader *header = Memory::header_from(p_ptr);
    const size_t size = header->size;

    Memory::record_free(size);
    free(header);
}
