#include "keyboard.h" // ПІДКЛЮЧАЄМО НАШ НОВИЙ ФАЙЛ
#include "io.h"
#include <stdint.h>

volatile char last_key_pressed = 0;
volatile bool key_ready = false;

// ---------------------------------------------------------------------------
// Scancode Set 1 — звичайний режим (без Shift)
// ---------------------------------------------------------------------------
static const char kbd_us[128] = {
/*00*/  0,
/*01*/  27,       // Escape
/*02*/  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
/*0C*/  '-', '=',
/*0E*/  '\b',     // Backspace
/*0F*/  '\t',     // Tab
/*10*/  'q','w','e','r','t','y','u','i','o','p',
/*1A*/  '[', ']',
/*1C*/  '\n',     // Enter
/*1D*/  0,        // Left Ctrl
/*1E*/  'a','s','d','f','g','h','j','k','l',
/*27*/  ';', '\'',
/*29*/  '`',
/*2A*/  0,        // Left Shift
/*2B*/  '\\',
/*2C*/  'z','x','c','v','b','n','m',
/*33*/  ',', '.', '/',
/*36*/  0,        // Right Shift
/*37*/  '*',
/*38*/  0,        // Left Alt
/*39*/  ' ',
/*3A*/  0,        // Caps Lock
/*3B*/  0,0,0,0,0,0,0,0,0,0, // F1-F10
/*45*/  0,        // Num Lock
/*46*/  0,        // Scroll Lock
/*47*/  0,0,0,    // Num 7,8,9
/*4A*/  '-',
/*4B*/  0,0,0,    // Num 4,5,6
/*4E*/  '+',
/*4F*/  0,0,0,    // Num 1,2,3
/*52*/  0,        // Num 0
/*53*/  0,        // Num .
/*54*/  0,0,0,0,0,0,0,0,0,0,0 // padding to 128
};

// ---------------------------------------------------------------------------
// Scancode Set 1 — Shift утримується
// ---------------------------------------------------------------------------
static const char kbd_us_shift[128] = {
/*00*/  0,
/*01*/  27,
/*02*/  '!','@','#','$','%','^','&','*','(',')',
/*0C*/  '_', '+',
/*0E*/  '\b',
/*0F*/  '\t',
/*10*/  'Q','W','E','R','T','Y','U','I','O','P',
/*1A*/  '{', '}',
/*1C*/  '\n',
/*1D*/  0,        // Ctrl
/*1E*/  'A','S','D','F','G','H','J','K','L',
/*27*/  ':', '"',
/*29*/  '~',
/*2A*/  0,        // Left Shift
/*2B*/  '|',
/*2C*/  'Z','X','C','V','B','N','M',
/*33*/  '<', '>', '?',
/*36*/  0,        // Right Shift
/*37*/  '*',
/*38*/  0,
/*39*/  ' ',
/*3A*/  0,
/*3B*/  0,0,0,0,0,0,0,0,0,0,
/*45*/  0,
/*46*/  0,
/*47*/  0,0,0,
/*4A*/  '-',
/*4B*/  0,0,0,
/*4E*/  '+',
/*4F*/  0,0,0,
/*52*/  0,
/*53*/  0,
/*54*/  0,0,0,0,0,0,0,0,0,0,0
};

static volatile uint8_t shift_pressed = 0;
static volatile uint8_t caps_lock_on  = 0;

static void kb_wait_write(void) {
    uint32_t i = 100000;
    while (--i && (inb(0x64) & 0x02));
}

static void kb_wait_read(void) {
    uint32_t i = 100000;
    while (--i && !(inb(0x64) & 0x01));
}

void keyboard_flush(void) {
    uint32_t i = 1000;
    while (--i && (inb(0x64) & 0x01)) {
        inb(0x60);
    }
}

void init_keyboard(void) {
    keyboard_flush();
    kb_wait_write();
    outb(0x64, 0xAE);
    kb_wait_write();
    outb(0x64, 0x20);
    kb_wait_read();
    uint8_t cmd = inb(0x60);
    cmd |= 0x01;
    cmd &= (uint8_t)~0x10;
    kb_wait_write();
    outb(0x64, 0x60);
    kb_wait_write();
    outb(0x60, cmd);
    keyboard_flush();
}

void keyboard_handler_main(void) {
    uint8_t keycode = inb(0x60);

    if (keycode & 0x80) {
        uint8_t sc = keycode & 0x7F;
        if (sc == 0x2A || sc == 0x36) shift_pressed = 0;
        outb(0x20, 0x20);
        return;
    }

    if (keycode == 0x2A || keycode == 0x36) {
        shift_pressed = 1;
        outb(0x20, 0x20);
        return;
    }

    if (keycode == 0x3A) {
        caps_lock_on ^= 1;
        outb(0x20, 0x20);
        return;
    }

    if (keycode >= 128) {
        outb(0x20, 0x20);
        return;
    }

    char c = shift_pressed ? kbd_us_shift[keycode] : kbd_us[keycode];

    if (caps_lock_on) {
        if (!shift_pressed && c >= 'a' && c <= 'z') c = (char)(c - 32);
        else if (shift_pressed && c >= 'A' && c <= 'Z') c = (char)(c + 32);
    }

    // НОВА ЛОГІКА: Передаємо символ ядру!
    if (c != 0) {
        last_key_pressed = c;
        key_ready = true;
    }

    outb(0x20, 0x20);
}