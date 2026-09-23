#include "kheap.h"

#define HEAP_ALIGNMENT 8U
#define HEAP_MAGIC 0xB16B00B5U

typedef struct header {
    uint32_t magic;
    size_t size;
    int is_free;
    struct header* next;
    struct header* prev;
} header_t;

static header_t* heap_start;
static uintptr_t heap_begin;
static uintptr_t heap_end;

static size_t align_size(size_t size) {
    return (size + HEAP_ALIGNMENT - 1U) & ~(HEAP_ALIGNMENT - 1U);
}

void init_kheap(uint32_t start_addr, uint32_t initial_size) {
    heap_begin = (start_addr + HEAP_ALIGNMENT - 1U) & ~(HEAP_ALIGNMENT - 1U);
    heap_end = (uintptr_t)start_addr + initial_size;
    heap_start = (header_t*)heap_begin;
    heap_start->magic = HEAP_MAGIC;
    heap_start->size = heap_end - heap_begin - sizeof(header_t);
    heap_start->is_free = 1;
    heap_start->next = 0;
    heap_start->prev = 0;
}

void* kmalloc(size_t requested) {
    if (!heap_start || requested == 0) return 0;
    size_t size = align_size(requested);

    for (header_t* block = heap_start; block; block = block->next) {
        if (block->magic != HEAP_MAGIC) return 0;
        if (!block->is_free || block->size < size) continue;

        if (block->size >= size + sizeof(header_t) + HEAP_ALIGNMENT) {
            header_t* split = (header_t*)((uintptr_t)(block + 1) + size);
            split->magic = HEAP_MAGIC;
            split->size = block->size - size - sizeof(header_t);
            split->is_free = 1;
            split->next = block->next;
            split->prev = block;
            if (split->next) split->next->prev = split;
            block->next = split;
            block->size = size;
        }

        block->is_free = 0;
        return block + 1;
    }
    return 0;
}

void* kcalloc(size_t count, size_t size) {
    if (count != 0 && size > (size_t)-1 / count) return 0;
    size_t total = count * size;
    uint8_t* ptr = (uint8_t*)kmalloc(total);
    if (!ptr) return 0;
    for (size_t i = 0; i < total; ++i) ptr[i] = 0;
    return ptr;
}

void kfree(void* ptr) {
    if (!ptr) return;
    uintptr_t address = (uintptr_t)ptr;
    if (address < heap_begin + sizeof(header_t) || address >= heap_end) return;

    header_t* block = ((header_t*)ptr) - 1;
    if (block->magic != HEAP_MAGIC || block->is_free) return;
    block->is_free = 1;

    if (block->next && block->next->magic == HEAP_MAGIC && block->next->is_free) {
        header_t* next = block->next;
        block->size += sizeof(header_t) + next->size;
        block->next = next->next;
        if (block->next) block->next->prev = block;
    }

    if (block->prev && block->prev->magic == HEAP_MAGIC && block->prev->is_free) {
        header_t* prev = block->prev;
        prev->size += sizeof(header_t) + block->size;
        prev->next = block->next;
        if (prev->next) prev->next->prev = prev;
    }
}
