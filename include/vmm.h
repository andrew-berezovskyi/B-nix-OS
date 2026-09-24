#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stdbool.h>

// Прапорці доступу до сторінки
#define PTE_PRESENT  0x01
#define PTE_RW       0x02
#define PTE_USER     0x04

extern uint32_t* kernel_directory;

bool init_vmm(void);

// 🔥 НОВІ ФУНКЦІЇ ЛІНУКС-СТАЙЛ:
// 1. Створює новий, чистий адресний простір для програми
uint32_t* vmm_create_address_space(void);

// 2. Мапить фізичну сторінку (4КБ) на віртуальну адресу в конкретному каталозі
void vmm_map_page(uint32_t* dir, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);

// 3. Змушує процесор перемкнутися на новий адресний простір (зміна CR3)
void vmm_switch_directory(uint32_t* dir);

#endif