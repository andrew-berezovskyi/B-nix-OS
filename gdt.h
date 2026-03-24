#ifndef GDT_H
#define GDT_H

#include <stdint.h>

// Структура одного запису в таблиці GDT (рівно 8 байт)
// __attribute__((packed)) забороняє компілятору додавати зайві байти для вирівнювання
struct gdt_entry_struct {
    uint16_t limit_low;           // Нижні 16 біт ліміту
    uint16_t base_low;            // Нижні 16 біт бази пам'яті
    uint8_t  base_middle;         // Наступні 8 біт бази
    uint8_t  access;              // Права доступу (Ring 0, Ring 3 тощо)
    uint8_t  granularity;         // Налаштування розміру (гранулярність)
    uint8_t  base_high;           // Останні 8 біт бази
} __attribute__((packed));
typedef struct gdt_entry_struct gdt_entry_t;

// Вказівник на GDT, який ми передамо процесору (рівно 6 байт)
struct gdt_ptr_struct {
    uint16_t limit;               // Розмір всієї таблиці в байтах
    uint32_t base;                // Адреса першого запису таблиці
} __attribute__((packed));
typedef struct gdt_ptr_struct gdt_ptr_t;

// Головна функція ініціалізації
void init_gdt(void);

#endif