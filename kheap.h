#ifndef KHEAP_H
#define KHEAP_H

#include <stdint.h>
#include <stddef.h>

// Ініціалізація купи
void init_kheap(uint32_t start_addr, uint32_t initial_size);

// Виділення пам'яті довільного розміру
void* kmalloc(size_t size);

// Звільнення пам'яті
void kfree(void* ptr);

#endif