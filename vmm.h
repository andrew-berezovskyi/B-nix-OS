#ifndef VMM_H
#define VMM_H

#include <stdint.h>

void init_vmm(void);
// ДОДАНО: функція для зв'язки віртуальної та фізичної адреси
void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr);
void vmm_map_page(uint32_t physical_addr, uint32_t virtual_addr);

#endif