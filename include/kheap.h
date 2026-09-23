#ifndef KHEAP_H
#define KHEAP_H

#include <stdint.h>
#include <stddef.h>

void init_kheap(uint32_t start_addr, uint32_t initial_size);
void* kmalloc(size_t size);
void* kcalloc(size_t count, size_t size);
void kfree(void* ptr);

#define STBTT_malloc(x,u) ((void)(u), kmalloc(x))
#define STBTT_free(x,u) ((void)(u), kfree(x))

#endif
