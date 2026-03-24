#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "shell.h"
#include "multiboot.h"
#include "pmm.h"
#include "vmm.h"
#include "kheap.h"
#include "vbe.h"
#include "rtc.h"
#include "mouse.h"
#include "fs.h" // ДОДАНО: Підключаємо файлову систему!

extern void init_keyboard(void);
extern void init_mouse(void);
extern void init_timer(uint32_t frequency);
extern void itoa(uint32_t num, char* str);

static uint32_t screen_w = 0;
static uint32_t screen_h = 0;
static uint8_t last_second = 255;

// Змінні вікна
bool win_open = false;
int win_x = 300;
int win_y = 200;
int win_w = 600;
int win_h = 400;
bool is_dragging = false;
int drag_offset_x = 0;
int drag_offset_y = 0;

void print_hex(uint32_t num) {
    char hex_str[11];
    hex_str[0] = '0'; hex_str[1] = 'x'; hex_str[10] = '\0';
    char hex_chars[] = "0123456789ABCDEF";
    for (int i = 9; i >= 2; i--) {
        hex_str[i] = hex_chars[num & 0xF]; num >>= 4;
    }
    print(hex_str);
}

static inline uint8_t inb_port(uint16_t port) {
    uint8_t ret; asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) ); return ret;
}

void wait_for_enter() {
    uint8_t key = 0;
    while (1) {
        if (inb_port(0x64) & 1) {
            key = inb_port(0x60);
            if (key == 0x1C) break;
        }
    }
}

// --- VGA ТЕКСТОВИЙ ДРАЙВЕР ---
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

typedef enum { VGA_COLOR_BLACK = 0, VGA_COLOR_LIGHT_GREEN = 10 } vga_color_t;
static size_t terminal_row; static size_t terminal_column;
static uint8_t terminal_color; static uint16_t* terminal_buffer;

static inline uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg) { return (uint8_t)(fg | (bg << 4)); }
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) { return (uint16_t)uc | ((uint16_t)color << 8); }

void terminal_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++) terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    terminal_row = 0; terminal_column = 0;
}
static void terminal_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++) terminal_buffer[y * VGA_WIDTH + x] = terminal_buffer[(y + 1) * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++) terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
}
void terminal_initialize(void) {
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_buffer = VGA_MEMORY; terminal_clear();
}
void terminal_putchar(char c) {
    if (c == '\n') { terminal_column = 0; if (++terminal_row == VGA_HEIGHT) { terminal_scroll(); terminal_row = VGA_HEIGHT - 1; } return; }
    if (c == '\r') { terminal_column = 0; return; }
    if (c == '\b') {
        if (terminal_column > 0) --terminal_column; else if (terminal_row > 0) { --terminal_row; terminal_column = VGA_WIDTH - 1; }
        terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = vga_entry(' ', terminal_color); return;
    }
    terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = vga_entry((unsigned char)c, terminal_color);
    if (++terminal_column == VGA_WIDTH) { terminal_column = 0; if (++terminal_row == VGA_HEIGHT) { terminal_scroll(); terminal_row = VGA_HEIGHT - 1; } }
}
void print(const char* str) { for (size_t i = 0; str[i] != '\0'; i++) terminal_putchar(str[i]); }

// --- GUI ФУНКЦІЇ ---
void draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < w; i++) { draw_pixel(x + i, y, color); draw_pixel(x + i, y + h - 1, color); }
    for (int i = 0; i < h; i++) { draw_pixel(x, y + i, color); draw_pixel(x + w - 1, y + i, color); }
}

void draw_filled_rect(int x, int y, int w, int h, uint32_t color) {
    for (int cy = y; cy < y + h; cy++)
        for (int cx = x; cx < x + w; cx++)
            draw_pixel(cx, cy, color);
}

// --- МАЛЮВАННЯ ВІКНА ТА ФАЙЛІВ ---
void draw_window() {
    if (!win_open) return;

    draw_filled_rect(win_x, win_y, win_w, win_h, 0x111111);
    draw_rect_outline(win_x, win_y, win_w, win_h, 0x555555);

    draw_filled_rect(win_x + 1, win_y + 1, win_w - 2, 28, 0x2A2A2A);
    draw_string(win_x + 10, win_y + 7, "File Explorer", 0xFFFFFF, 1);

    int close_btn_x = win_x + win_w - 30;
    int close_btn_y = win_y + 1;
    draw_filled_rect(close_btn_x, close_btn_y, 29, 28, 0xAA3333); 
    draw_string(close_btn_x + 11, close_btn_y + 7, "X", 0xFFFFFF, 1);

    // ВІДМАЛЬОВКА ФАЙЛОВОЇ СИСТЕМИ
    int file_x = win_x + 30;
    int file_y = win_y + 50;

    for (int i = 0; i < MAX_FILES; i++) {
        if (virtual_fs[i].is_used) {
            // Малюємо іконку файлу (аркуш паперу)
            draw_filled_rect(file_x, file_y, 32, 40, 0xDDDDDD);
            // Малюємо загин на папері для стилю
            draw_filled_rect(file_x + 24, file_y, 8, 8, 0xAAAAAA);
            
            // Малюємо назву файлу під іконкою
            draw_string(file_x - 10, file_y + 45, virtual_fs[i].name, 0xFFFFFF, 1);

            // Зсуваємо координати для наступного файлу вправо
            file_x += 100;
            // Якщо файли не влазять у вікно по ширині, переносимо на новий рядок
            if (file_x > win_x + win_w - 80) {
                file_x = win_x + 30;
                file_y += 80;
            }
        }
    }
}

void draw_start_menu(uint32_t width, uint32_t height) {
    int menu_w = 300; int menu_h = 420;
    int menu_x = (width / 2) - (menu_w / 2); int menu_y = height - 45 - menu_h;

    draw_filled_rect(menu_x, menu_y, menu_w, menu_h, 0x111111);
    draw_rect_outline(menu_x, menu_y, menu_w, menu_h, 0x333333);

    draw_filled_rect(menu_x + 15, menu_y + 15, menu_w - 30, 30, 0x222222);
    draw_string(menu_x + 25, menu_y + 22, "Type to search...", 0x888888, 1);
    draw_string(menu_x + 15, menu_y + 65, "Pinned Apps", 0xFFFFFF, 1);
    
    draw_rect_outline(menu_x + 15, menu_y + 90, 40, 40, 0x555555);
    draw_string(menu_x + 23, menu_y + 102, "DIR", 0xFFFFFF, 1);
}

void draw_desktop(uint32_t width, uint32_t height) {
    draw_string((width / 2) - 192, (height / 2) - 48, "B-nix OS", 0x222222, 6);
    for (int x = 0; x < width; x++) draw_pixel(x, height - 40, 0xFFFFFF); 
    draw_string((width / 2) - 40, height - 36, "B-nix", 0xFFFFFF, 2);
}

void update_clock(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    uint8_t h, m, s, d, mo; uint32_t y;
    read_rtc(&h, &m, &s, &d, &mo, &y);

    char time_str[6] = "00:00";
    time_str[0] = (h / 10) + '0'; time_str[1] = (h % 10) + '0';
    time_str[3] = (m / 10) + '0'; time_str[4] = (m % 10) + '0';
    char date_str[11] = "00/00/0000";
    date_str[0] = (d / 10) + '0'; date_str[1] = (d % 10) + '0';
    date_str[3] = (mo / 10) + '0'; date_str[4] = (mo % 10) + '0';
    date_str[6] = (y / 1000) + '0'; date_str[7] = ((y / 100) % 10) + '0';
    date_str[8] = ((y / 10) % 10) + '0'; date_str[9] = (y % 10) + '0';

    draw_string(width - 90, height - 32, time_str, 0xFFFFFF, 1);
    draw_string(width - 90, height - 16, date_str, 0xAAAAAA, 1);
}

// --- ГОЛОВНА ФУНКЦІЯ ---
void kernel_main(uint32_t magic, multiboot_info_t* mbd) {
    init_gdt(); init_idt();

    if (magic == 0x2BADB002 && (mbd->flags & (1 << 12))) {
        uint32_t fb_addr = (uint32_t)(mbd->framebuffer_addr & 0xFFFFFFFF);
        screen_w = mbd->framebuffer_width; screen_h = mbd->framebuffer_height;
        init_graphics((uint32_t*)fb_addr, screen_w, screen_h, mbd->framebuffer_pitch, mbd->framebuffer_bpp);

        clear_screen(0x000000);
        int text_x = (screen_w / 2) - 304; int text_y = (screen_h / 2) - 64;
        draw_string(text_x, text_y, "Welcome to B-nix OS", 0xFFFFFF, 4);
        draw_string(text_x + 96, text_y + 120, "Press ENTER to continue...", 0x888888, 2);
        swap_buffers(); wait_for_enter();
    }

    init_keyboard(); init_mouse(); init_timer(100); init_shell();
    
    // ІНІЦІАЛІЗУЄМО НАШУ ФАЙЛОВУ СИСТЕМУ
    init_vfs();

    bool prev_click = false;
    bool show_start_menu = false;

    while (1) {
        clear_screen(0x000000);
        draw_desktop(screen_w, screen_h);
        update_clock(screen_w, screen_h);

        bool just_clicked = (mouse_left_pressed && !prev_click);

        if (win_open) {
            int close_btn_x = win_x + win_w - 30;
            if (just_clicked && mouse_y >= win_y && mouse_y <= win_y + 28 && mouse_x >= close_btn_x && mouse_x <= win_x + win_w) {
                win_open = false;
            }
            else if (just_clicked && mouse_y >= win_y && mouse_y <= win_y + 28 && mouse_x >= win_x && mouse_x <= win_x + win_w) {
                is_dragging = true;
                drag_offset_x = mouse_x - win_x;
                drag_offset_y = mouse_y - win_y;
            }
        }

        if (is_dragging) {
            if (mouse_left_pressed) {
                win_x = mouse_x - drag_offset_x;
                win_y = mouse_y - drag_offset_y;
            } else {
                is_dragging = false; 
            }
        }

        int btn_w = 90; int btn_h = 36;
        int btn_x = (screen_w / 2) - (btn_w / 2); int btn_y = screen_h - 38;
        bool hover_bnix = (mouse_x >= btn_x && mouse_x <= btn_x + btn_w && mouse_y >= btn_y && mouse_y <= btn_y + btn_h);

        if (hover_bnix) draw_rect_outline(btn_x, btn_y, btn_w, btn_h, 0x555555);

        if (just_clicked) {
            if (hover_bnix) {
                show_start_menu = !show_start_menu;
            } 
            else if (show_start_menu) {
                int menu_w = 300; int menu_h = 420;
                int menu_x = (screen_w / 2) - (menu_w / 2); int menu_y = screen_h - 45 - menu_h;
                
                if (mouse_x >= menu_x + 15 && mouse_x <= menu_x + 55 && mouse_y >= menu_y + 90 && mouse_y <= menu_y + 130) {
                    win_open = true; 
                    show_start_menu = false; 
                }
            }
        }

        prev_click = mouse_left_pressed;

        draw_window();
        if (show_start_menu) draw_start_menu(screen_w, screen_h);
        draw_cursor(mouse_x, mouse_y);

        swap_buffers();
        asm volatile("hlt");
    }
}