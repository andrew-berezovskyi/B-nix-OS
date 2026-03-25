#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// Структура одного запису в IDT
struct idt_entry_struct {
    uint16_t base_low;             // Нижні 16 біт адреси функції-обробника
    uint16_t sel;                  // Сегмент коду (з нашої GDT)
    uint8_t  always0;              // Цей байт завжди має бути 0
    uint8_t  flags;                // Права доступу
    uint16_t base_high;            // Верхні 16 біт адреси
} __attribute__((packed));
typedef struct idt_entry_struct idt_entry_t;

// Вказівник на таблицю IDT для процесора
struct idt_ptr_struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));
typedef struct idt_ptr_struct idt_ptr_t;

void init_idt(void);

// Оголошуємо наш новий шлюз з boot.asm
extern void syscall_handler(void);

#endif