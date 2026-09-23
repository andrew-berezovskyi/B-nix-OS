#include "serial.h"
#include "io.h"

#define COM1 0x3F8

static int serial_ready(void) {
    return (inb(COM1 + 5) & 0x20) != 0;
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_write_char(char c) {
    uint32_t timeout = 100000;
    while (!serial_ready() && --timeout) {}
    if (timeout) outb(COM1, (uint8_t)c);
}

void serial_write(const char* text) {
    if (!text) return;
    while (*text) serial_write_char(*text++);
}

void serial_write_hex(uint32_t value) {
    static const char digits[] = "0123456789ABCDEF";
    serial_write("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        serial_write_char(digits[(value >> shift) & 0xF]);
    }
}
