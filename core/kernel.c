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
#include "fs.h"
#include "keyboard.h"
#include "timer.h"
#include "desktop.h"

uint8_t* main_font_data = NULL;
uint8_t* bg_image_data = NULL; uint32_t bg_image_size = 0;
uint8_t* icon_image_data = NULL; uint32_t icon_image_size = 0;

// Оголошуємо функції з vbe.c, щоб ядро їх бачило
extern bool cache_background_image(uint8_t* img_data, uint32_t img_size);
extern bool cache_icon_image(uint8_t* img_data, uint32_t img_size);
extern bool cache_folder_image(uint8_t* img_data, uint32_t img_size);
extern bool cache_file_image(uint8_t* img_data, uint32_t img_size);

void* memset(void* dest, int ch, size_t count) {
    unsigned char* ptr = (unsigned char*)dest;
    while (count--) *ptr++ = (unsigned char)ch;
    return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (count--) *d++ = *s++;
    return dest;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

void* __memset_chk(void* dest, int ch, size_t count, size_t destlen) {
    (void)destlen; return memset(dest, ch, count);
}

void* __memcpy_chk(void* dest, const void* src, size_t len, size_t destlen) {
    (void)destlen; return memcpy(dest, src, len);
}

static uint32_t screen_w = 0;
static uint32_t screen_h = 0;

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

void terminal_clear(void) {
    terminal_buffer = VGA_MEMORY;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = (uint16_t)' ' | ((uint16_t)terminal_color << 8);
        }
    }
    terminal_row = 0; terminal_column = 0;
}

static void vga_terminal_putchar(char c) {
    if (c == '\n') { terminal_column = 0; if (++terminal_row == VGA_HEIGHT) terminal_row = 0; return; }
    terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = (uint16_t)c | ((uint16_t)terminal_color << 8);
    if (++terminal_column == VGA_WIDTH) { terminal_column = 0; if (++terminal_row == VGA_HEIGHT) terminal_row = 0; }
}

void terminal_putchar(char c) { vga_terminal_putchar(c); }

void print(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (current_state == STATE_DESKTOP) term_gui_feed(str[i]);
        vga_terminal_putchar(str[i]);
    }
}

void kernel_main(uint32_t magic, multiboot_info_t* mbd) {
    init_gdt(); 
    init_idt();
    init_kheap(0x1000000, 16 * 1024 * 1024);
    terminal_color = 0x0A; 

    if (magic == 0x2BADB002 && (mbd->flags & (1 << 12))) {
        uint32_t fb_addr = (uint32_t)(mbd->framebuffer_addr & 0xFFFFFFFF); 
        screen_w = mbd->framebuffer_width; 
        screen_h = mbd->framebuffer_height; 
        init_graphics((uint32_t*)fb_addr, screen_w, screen_h, mbd->framebuffer_pitch, mbd->framebuffer_bpp);
        
        if (mbd->flags & (1 << 3)) {
            multiboot_module_t* mod = (multiboot_module_t*)mbd->mods_addr;
            uint32_t mod_count = mbd->mods_count;

            // Зчитуємо файли по черзі (як вони вказані в grub.cfg)
            if (mod_count > 0) main_font_data = (uint8_t*)mod[0].mod_start;
            
            if (mod_count > 1) { 
                bg_image_data = (uint8_t*)mod[1].mod_start; 
                bg_image_size = mod[1].mod_end - mod[1].mod_start; 
            }
            if (mod_count > 2) { 
                icon_image_data = (uint8_t*)mod[2].mod_start; 
                icon_image_size = mod[2].mod_end - mod[2].mod_start; 
            }
            
            // НОВЕ: Завантажуємо іконки папок і файлів!
            if (mod_count > 3) {
                uint8_t* folder_data = (uint8_t*)mod[3].mod_start;
                uint32_t folder_size = mod[3].mod_end - mod[3].mod_start;
                cache_folder_image(folder_data, folder_size);
            }
            if (mod_count > 4) {
                uint8_t* file_data = (uint8_t*)mod[4].mod_start;
                uint32_t file_size = mod[4].mod_end - mod[4].mod_start;
                cache_file_image(file_data, file_size);
            }
        }
    } 

    init_keyboard(); init_mouse(); init_timer(100); init_shell(); init_vfs();
    
    if (bg_image_data) cache_background_image(bg_image_data, bg_image_size);
    if (icon_image_data) cache_icon_image(icon_image_data, icon_image_size);

    desktop_init(screen_w, screen_h);

    bool prev_c = false; bool prev_r = false;
    uint32_t last_tick = timer_ticks;

    while (1) {
        asm volatile("hlt");
        if (timer_ticks != last_tick) {
            last_tick = timer_ticks;

            int mx = mouse_x; int my = mouse_y;
            bool left_now = mouse_left_pressed; bool right_now = mouse_right_pressed;
            bool j_c = (left_now && !prev_c); bool j_r = (right_now && !prev_r);

            if (key_ready) {
                desktop_handle_keypress(last_key_pressed);
                key_ready = false;
            }

            desktop_process_mouse(mx, my, left_now, right_now, j_c, j_r);
            desktop_draw();
            
            draw_cursor(mx, my); 
            swap_buffers();
            
            prev_c = left_now; prev_r = right_now;
        }
    }
}