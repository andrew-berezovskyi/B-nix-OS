#include "rtc.h"
#include "io.h"

// Перетворення формату BCD у звичайні числа
static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd / 16) * 10);
}

void read_rtc(uint8_t* h, uint8_t* m, uint8_t* s, uint8_t* d, uint8_t* mo, uint32_t* y) {
    outb(0x70, 0x00); // Секунди
    *s = bcd_to_bin(inb(0x71));
    
    outb(0x70, 0x02); // Хвилини
    *m = bcd_to_bin(inb(0x71));
    
    outb(0x70, 0x04); // Години
    *h = bcd_to_bin(inb(0x71));
    
    outb(0x70, 0x07); // День
    *d = bcd_to_bin(inb(0x71));
    
    outb(0x70, 0x08); // Місяць
    *mo = bcd_to_bin(inb(0x71));
    
    outb(0x70, 0x09); // Рік (повертає двозначне число, наприклад 26)
    *y = bcd_to_bin(inb(0x71)) + 2000; 
}