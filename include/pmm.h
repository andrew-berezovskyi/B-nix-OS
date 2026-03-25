#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PMM_BLOCK_SIZE 4096 // Розмір одного блоку пам'яті (4 КБ)

// Ініціалізуємо менеджер, передаючи йому загальний розмір пам'яті в кілобайтах
void init_pmm(uint32_t mem_size_kb);

// Знайти і виділити один вільний блок (повертає фізичну адресу блоку)
uint32_t pmm_alloc_block(void);

// Звільнити блок за його адресою
void pmm_free_block(uint32_t address);

// Отримати кількість вільної пам'яті (в блоках)
uint32_t pmm_get_free_block_count(void);

uint32_t pmm_get_max_blocks(void);

#endif