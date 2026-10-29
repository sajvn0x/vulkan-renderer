#pragma once

#include <cstddef>
#include <cstdint>

namespace Memory {

struct Statistics {
    uint64_t current_bytes = 0;
    uint64_t peak_bytes = 0;

    uint64_t live_allocations = 0;
    uint64_t total_allocations = 0;
    uint64_t total_frees = 0;
    uint64_t failed_allocations = 0;
};

Memory::Statistics get_statistics();
void print_statistics();

};  // namespace Memory

void *mem_alloc(size_t p_size);
void *mem_calloc(size_t p_count, size_t p_size);
void *mem_realloc(void *p_ptr, size_t p_new_size);
void mem_free(void *p_ptr);

#define memnew(m_class) (m_class *)mem_alloc(sizeof(m_class))

#define memnew_arr(m_class, m_amount) (m_class *)mem_alloc(sizeof(m_class) * m_amount)

#define memdelete(m_ptr) mem_free(m_ptr)
