#include "idt.h"
#include "io.h"

idt_entry_t idt_entries[256];
idt_ptr_t   idt_ptr;

extern void idt_flush(uint32_t);
extern void keyboard_handler(void);
extern void timer_handler(void);
extern void mouse_handler(void); // ДОДАНО: обробник миші

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_low  = (base & 0xFFFF);
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].sel     = sel;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags   = flags;
}

void init_idt(void) {
    idt_ptr.limit = (sizeof(idt_entry_t) * 256) - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // ЗМІНЕНО: 0xF8 розблоковує IRQ0(Таймер), IRQ1(Клава) та IRQ2(Каскад для миші)
    outb(0x21, 0xF8); 
    // ЗМІНЕНО: 0xEF розблоковує IRQ12 на другому контролері (Миша)
    outb(0xA1, 0xEF); 

    idt_set_gate(32, (uint32_t)timer_handler, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)keyboard_handler, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)mouse_handler, 0x08, 0x8E); // IRQ12 -> 44

    idt_flush((uint32_t)&idt_ptr);
    asm volatile("sti");
}