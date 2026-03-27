#include "vmm.h"
#include "kheap.h" // Для kmalloc
#include <stdint.h>
#include <stddef.h>

extern void print(const char* str);

uint32_t* kernel_directory = NULL;

// Допоміжна функція для виділення вирівняної пам'яті (вкрай важливо для VMM!)
static void* kmalloc_aligned(uint32_t size) {
    uint32_t addr = (uint32_t)kmalloc(size + 4096);
    // Округлюємо адресу до найближчого кратного 4096 (4КБ)
    return (void*)((addr + 4095) & ~0xFFF);
}

void init_vmm(void) {
    kernel_directory = (uint32_t*)kmalloc_aligned(4096);

    // 1. Вмикаємо PSE, щоб ядро могло використовувати гігантські сторінки по 4 МБ
    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x00000010; 
    asm volatile("mov %0, %%cr4" :: "r"(cr4));

    // 2. Мапуємо ВСІ 4 Гігабайти простору для ядра (Identity Mapping 4MB pages)
    for (int i = 0; i < 1024; i++) {
        // 0x80 = 4MB сторінка, 0x02 = RW, 0x01 = Present
        kernel_directory[i] = (i << 22) | 0x83;
    }

    // 3. Завантажуємо адресу каталогу в CR3
    vmm_switch_directory(kernel_directory);

    // 4. Вмикаємо Paging
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

// Створює "клон" адресного простору ядра для нової програми
uint32_t* vmm_create_address_space(void) {
    uint32_t* dir = (uint32_t*)kmalloc_aligned(4096);
    
    // Копіюємо ядро, щоб програма могла викликати переривання і бачити відеопам'ять
    for (int i = 0; i < 1024; i++) {
        dir[i] = kernel_directory[i];
    }
    
    return dir;
}

// Справжнє мапування 4КБ сторінок
void vmm_map_page(uint32_t* dir, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    // Якщо в цьому мегабайті ще немає 4КБ-таблиці (або там лежить 4MB заглушка ядра 0x80)
    if ((dir[pd_index] & 0x01) == 0 || (dir[pd_index] & 0x80)) {
        uint32_t* pt = (uint32_t*)kmalloc_aligned(4096);
        for (int i = 0; i < 1024; i++) pt[i] = 0; // Чистимо таблицю
        
        // Вставляємо нову таблицю в каталог (без прапорця 4MB!)
        dir[pd_index] = ((uint32_t)pt) | PTE_PRESENT | PTE_RW | PTE_USER;
    }

    // Отримуємо адресу самої таблиці сторінок і мапимо фізичну сторінку
    uint32_t* pt = (uint32_t*)(dir[pd_index] & ~0xFFF);
    pt[pt_index] = (physical_addr & ~0xFFF) | (flags & 0xFFF) | PTE_PRESENT;
}

void vmm_switch_directory(uint32_t* dir) {
    asm volatile("mov %0, %%cr3" :: "r"(dir));
}