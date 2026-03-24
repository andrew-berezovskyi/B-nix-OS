#include "kheap.h"

typedef struct header {
    size_t size;
    int is_free;
    struct header *next;
} header_t;

static header_t *heap_start = NULL;

void init_kheap(uint32_t start_addr, uint32_t initial_size) {
    heap_start = (header_t*)start_addr;
    heap_start->size = initial_size - sizeof(header_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;
}

void* kmalloc(size_t size) {
    header_t *curr = heap_start;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            // Якщо блок значно більший, розбиваємо його
            if (curr->size > size + sizeof(header_t) + 4) {
                header_t *new_block = (header_t*)((uint32_t)curr + sizeof(header_t) + size);
                new_block->size = curr->size - size - sizeof(header_t);
                new_block->is_free = 1;
                new_block->next = curr->next;
                
                curr->size = size;
                curr->next = new_block;
            }
            curr->is_free = 0;
            return (void*)((uint32_t)curr + sizeof(header_t));
        }
        curr = curr->next;
    }
    return NULL; // Out of memory
}

void kfree(void* ptr) {
    if (!ptr) return;
    header_t *header = (header_t*)((uint32_t)ptr - sizeof(header_t));
    header->is_free = 1;
    // Склейку вільних блоків (Coalescing) пропустимо для простоти, 
    // для базових задач цього вистачить.
}