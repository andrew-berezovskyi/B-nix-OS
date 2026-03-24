#include "vmm.h"
#include <stdint.h>

extern void print(const char* str);

// Тепер нам потрібен лише один каталог, без купи дрібних таблиць
uint32_t page_directory[1024] __attribute__((aligned(4096)));

void init_vmm(void) {
    // 1. Вмикаємо PSE (Page Size Extension), щоб використовувати гігантські сторінки по 4 МБ
    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x00000010; // Встановлюємо 4-й біт (PSE Enable)
    asm volatile("mov %0, %%cr4" :: "r"(cr4));

    // 2. Мапуємо ВСІ 4 Гігабайти простору (Identity Mapping)
    for (int i = 0; i < 1024; i++) {
        // Атрибути: 
        // 0x80 - це сторінка на 4 МБ
        // 0x02 - Read/Write
        // 0x01 - Present
        // Разом = 0x83
        page_directory[i] = (i << 22) | 0x83;
    }

    // 3. Завантажуємо адресу каталогу в CR3
    asm volatile("mov %0, %%cr3" :: "r"(page_directory));

    // 4. Вмикаємо Paging
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

// Функція-заглушка: тепер вона нам не потрібна, бо вся пам'ять вже доступна
void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr) {
    // Нічого не робимо
}