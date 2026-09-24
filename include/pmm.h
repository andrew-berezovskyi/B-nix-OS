#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PMM_BLOCK_SIZE 4096U

void init_pmm(uint32_t mem_size_kb);
void pmm_reserve_region(uint32_t base, uint32_t size);
void pmm_free_region(uint32_t base, uint32_t size);
uint32_t pmm_alloc_block(void);
void pmm_free_block(uint32_t address);
uint32_t pmm_get_free_block_count(void);
uint32_t pmm_get_max_blocks(void);

#endif
