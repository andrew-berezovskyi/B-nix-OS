#include "gdt.h"

// Створюємо масив з 3 записів: Null, Code, Data
gdt_entry_t gdt_entries[3];
gdt_ptr_t   gdt_ptr;

// Зовнішня функція на Асемблері (ми додамо її в boot.asm)
extern void gdt_flush(uint32_t);

// Допоміжна функція для зручного заповнення одного сегмента
void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

// Головна функція, яку ми викличемо з ядра
void init_gdt(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 3) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    // 1. Нульовий сегмент
    gdt_set_gate(0, 0, 0, 0, 0);

    // 2. Сегмент коду: починається з 0, розмір 4 ГБ, Ring 0 (0x9A), 32-бітний (0xCF)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // 3. Сегмент даних: такі ж параметри, але інші права (0x92 - дозволено запис)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // Кажемо процесору: "Завантаж нову карту пам'яті!"
    gdt_flush((uint32_t)&gdt_ptr);
}