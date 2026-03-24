#include <stdint.h>
#include <stddef.h>
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "shell.h"
#include "multiboot.h"
#include "pmm.h"
#include "vmm.h"
#include "kheap.h"
#include "vbe.h"

extern void init_keyboard(void);
extern void init_mouse(void);
extern void init_timer(uint32_t frequency);
extern void itoa(uint32_t num, char* str);

// Global screen size
static uint32_t screen_w = 0;
static uint32_t screen_h = 0;

// --- HELPERS ---
void print_hex(uint32_t num) {
    char hex_str[11];
    hex_str[0] = '0';
    hex_str[1] = 'x';
    hex_str[10] = '\0';
    char hex_chars[] = "0123456789ABCDEF";
    for (int i = 9; i >= 2; i--) {
        hex_str[i] = hex_chars[num & 0xF];
        num >>= 4;
    }
    print(hex_str);
}

// Reading from port
static inline uint8_t inb_port(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// --- WAIT FOR ENTER FUNCTION ---
void wait_for_enter() {
    uint8_t key = 0;
    while (1) {
        if (inb_port(0x64) & 1) {
            key = inb_port(0x60);
            if (key == 0x1C) {
                break;
            }
        }
    }
}

// --- VGA TEXT DRIVER ---
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_YELLOW        = 14,
    VGA_COLOR_WHITE         = 15,
} vga_color_t;

static size_t    terminal_row;
static size_t    terminal_column;
static uint8_t   terminal_color;
static uint16_t* terminal_buffer;

static inline uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg) {
    return (uint8_t)(fg | (bg << 4));
}
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)uc | ((uint16_t)color << 8);
}

void terminal_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
        }
    }
    terminal_row = 0;
    terminal_column = 0;
}

static void terminal_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = terminal_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
}

void terminal_initialize(void) {
    terminal_color  = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_buffer = VGA_MEMORY;
    terminal_clear();
}

void terminal_set_color(vga_color_t fg, vga_color_t bg) {
    terminal_color = vga_entry_color(fg, bg);
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) { terminal_scroll(); terminal_row = VGA_HEIGHT - 1; }
        return;
    }
    if (c == '\r') { terminal_column = 0; return; }
    if (c == '\b') {
        if (terminal_column > 0) --terminal_column;
        else if (terminal_row > 0) { --terminal_row; terminal_column = VGA_WIDTH - 1; }
        terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = vga_entry(' ', terminal_color);
        return;
    }
    if (c == '\t') {
        terminal_column = (terminal_column + 8) & ~(size_t)7;
        if (terminal_column >= VGA_WIDTH) {
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT) { terminal_scroll(); terminal_row = VGA_HEIGHT - 1; }
        }
        return;
    }
    terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = vga_entry((unsigned char)c, terminal_color);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) { terminal_scroll(); terminal_row = VGA_HEIGHT - 1; }
    }
}

void print(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) terminal_putchar(str[i]);
}

// --- DRAW DESKTOP FUNCTION ---
void draw_desktop(uint32_t width, uint32_t height) {
    clear_screen(0x000000); // Black background

    // 1. Draw minimal background watermark/logo (Kali style)
    // Text "B-nix OS" Scale 6, center of screen (1280x720)
    // Char width Scale 6 = 8*6=48. Char height Scale 6 = 16*6=96.
    // Total Width (8 chars) = 8*48 = 384.
    // Center X = (1280 - 384) / 2 = 448. Center Y = (720 - 96) / 2 = 312.
    // User requested very low contrast gray (ледфе сірим).
    draw_string(448, 312, "B-nix OS", 0x222222, 6); // Extremely low visibility gray

    // 2. Draw minimal taskbar line
    for (int x = 0; x < width; x++) {
        draw_pixel(x, height - 40, 0xFFFFFF); // White separator line 1px high
    }

    // 3. Draw B-nix label, center of taskbar line
    // Line is at Y=680 (1280x720, height-40).
    // Usable space is Y=681 to Y=719 (39px height).
    // Font Scale 2 height = 16*2=32px.
    // To vertically center 32px text in 39px area: padding is (39-32)/2 = 3.5.
    // Let's use 3 pixels padding top/bottom relative to unusable top 1px line.
    // Top text start Y = 681 + 3 = 684. Or calculated from bottom: height - 36 (720-36=684).
    draw_string((width / 2) - 40, height - 36, "B-nix", 0xFFFFFF, 2); // White text

    // 4. Draw Right Status Area (Static Placeholder)
    // Using Scale 1 (VGA 8x16) to fit vertically in 39px, but will align slightly better
    // with the bottom to look modern and avoid overlap.
    // User: "00:00" and "24/03/2026"
    // Taskbar Area Y=681 to Y=719 (height-39 to height-1).
    draw_string(width - 120, height - 32, "00:00", 0xFFFFFF, 1); // Top line, White
    draw_string(width - 136, height - 16, "24/03/2026", 0xAAAAAA, 1); // Bottom line, Gray
}

// --- KERNEL MAIN FUNCTION ---
void kernel_main(uint32_t magic, multiboot_info_t* mbd) {
    init_gdt();
    init_idt();

    if (magic == 0x2BADB002 && (mbd->flags & (1 << 12))) {
        uint32_t fb_addr = (uint32_t)(mbd->framebuffer_addr & 0xFFFFFFFF);
        screen_w = mbd->framebuffer_width;
        screen_h = mbd->framebuffer_height;

        init_graphics((uint32_t*)fb_addr, screen_w, screen_h, mbd->framebuffer_pitch, mbd->framebuffer_bpp);

        clear_screen(0x000000);

        int text_x = (screen_w / 2) - 304;
        int text_y = (screen_h / 2) - 64;
        draw_string(text_x, text_y, "Welcome to B-nix OS", 0xFFFFFF, 4);

        int hint_x = (screen_w / 2) - 208;
        int hint_y = text_y + 120;
        draw_string(hint_x, hint_y, "Press ENTER to continue...", 0x888888, 2);

        wait_for_enter();

        draw_desktop(screen_w, screen_h);
    }

    init_keyboard();
    init_mouse();
    init_timer(100);
    init_shell();

    while (1) {
        asm volatile("hlt");
    }
}