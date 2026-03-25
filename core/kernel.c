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

extern bool cache_background_image(uint8_t* img_data, uint32_t img_size);
extern bool cache_icon_image(uint8_t* img_data, uint32_t img_size);
extern bool cache_folder_image(uint8_t* img_data, uint32_t img_size);
extern bool cache_file_image(uint8_t* img_data, uint32_t img_size);
extern void draw_filled_rect(int x, int y, int w, int h, uint32_t color);

// Беремо змінну is_dragging з wm.c, щоб знати, чи тягнемо ми вікно
extern bool is_dragging;

// --- СТАНДАРТНІ ФУНКЦІЇ ---
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
    size_t len = 0; while (str[len]) len++; return len;
}
void* __memset_chk(void* dest, int ch, size_t count, size_t destlen) {
    (void)destlen; return memset(dest, ch, count);
}
void* __memcpy_chk(void* dest, const void* src, size_t len, size_t destlen) {
    (void)destlen; return memcpy(dest, src, len);
}

// --- ТЕРМІНАЛ ---
static uint32_t screen_w = 0;
static uint32_t screen_h = 0;
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

void terminal_clear(void) {
    uint16_t* buf = VGA_MEMORY;
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) buf[i] = (uint16_t)' ' | (0x0A << 8);
}
void terminal_putchar(char c) {
    static int col = 0, row = 0; uint16_t* buf = VGA_MEMORY;
    if (c == '\n') { col = 0; row++; }
    else { buf[row * VGA_WIDTH + col] = (uint16_t)c | (0x0A << 8); col++; if (col >= VGA_WIDTH) { col = 0; row++; } }
    if (row >= VGA_HEIGHT) row = 0;
}
void print(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (current_state == STATE_DESKTOP) term_gui_feed(str[i]);
        terminal_putchar(str[i]);
    }
}

// ==============================================================================
// 🔥 СИСТЕМА НЕЗАЛЕЖНОГО КУРСОРУ 
// ==============================================================================
static uint32_t cursor_bg_save[32 * 32];
static int saved_mx = -1, saved_my = -1;

extern uint32_t get_pixel(int x, int y);
extern void draw_pixel(int x, int y, uint32_t color);

static void save_cursor_bg(int mx, int my) {
    saved_mx = mx; saved_my = my;
    for (int cy = 0; cy < 32; cy++) {
        for (int cx = 0; cx < 32; cx++) {
            cursor_bg_save[cy * 32 + cx] = get_pixel(mx + cx, my + cy);
        }
    }
}

static void erase_cursor(void) {
    if (saved_mx < 0) return;
    for (int cy = 0; cy < 32; cy++) {
        for (int cx = 0; cx < 32; cx++) {
            draw_pixel(saved_mx + cx, saved_my + cy, cursor_bg_save[cy * 32 + cx]);
        }
    }
}

// Глобальна змінна для зв'язку між фоновою задачею та GUI
volatile int blinker_color = 0; // 0=Вимкнено, 1=Червоний, 2=Зелений

// ==============================================================================
// ЗАВДАННЯ 1: GUI (Графічний інтерфейс)
// ==============================================================================
void task_gui_main(void) {
    bool prev_c = false, prev_r = false;
    int last_mx = -1, last_my = -1;
    uint32_t last_full_draw = 0;
    bool is_first_run = true; 

    while (1) {
        asm volatile("hlt"); // Спимо до переривання

        int mx = mouse_x; int my = mouse_y;
        bool left_now = mouse_left_pressed; bool right_now = mouse_right_pressed;

        bool mouse_moved = (mx != last_mx || my != last_my);
        bool click_changed = (left_now != prev_c || right_now != prev_r);
        bool j_c = (left_now && !prev_c); bool j_r = (right_now && !prev_r);
        bool has_key = key_ready;

        // 1. Оновлюємо логіку вікон (координати, кліки), якщо щось сталося
        if (mouse_moved || click_changed || has_key) {
            desktop_process_mouse(mx, my, left_now, right_now, j_c, j_r);
        }
        if (has_key) { 
            desktop_handle_keypress(last_key_pressed); 
            key_ready = false; 
        }

        // 2. ЖОРСТКИЙ FPS CAP: Чи пройшов хоча б 1 тік таймера?
        bool time_passed = (timer_ticks != last_full_draw);
        
        // Ми перемальовуємо важкі вікна ТІЛЬКИ ЯКЩО:
        // - Це перший запуск
        // - Клікнули мишею
        // - Натиснули клавіатуру
        // - Ми тягнемо вікно, миша рухається, І ПРОЙШОВ ТІК ТАЙМЕРА (максимум 50 FPS)
        // - Пройшло 0.5 секунди (оновлення годинника/індикатора)
        bool dragging_movement = (is_dragging && mouse_moved && time_passed);
        bool full_redraw = is_first_run || click_changed || has_key || dragging_movement || (timer_ticks - last_full_draw > 25);

        if (full_redraw) {
            is_first_run = false;
            last_full_draw = timer_ticks;

            saved_mx = -1; 
            desktop_draw(); // Малюємо вікна і фон
            
            // МАГІЯ: Тепер GUI малює індикатор, гарантуючи, що він буде ЗВЕРХУ!
            if (blinker_color == 1) draw_filled_rect(1250, 5, 20, 20, 0xFF0000);
            else if (blinker_color == 2) draw_filled_rect(1250, 5, 20, 20, 0x00FF00);
            
            save_cursor_bg(mx, my);
            draw_cursor(mx, my);
            swap_buffers(); // Відправляємо готовий ідеальний кадр на екран

        } else if (mouse_moved) {
            // Легке малювання (Тільки курсор миші - без FPS Cap)
            erase_cursor();         
            save_cursor_bg(mx, my); 
            draw_cursor(mx, my);    
            swap_buffers();         
        }

        last_mx = mx; last_my = my;
        prev_c = left_now; prev_r = right_now;
    }
}

// ==============================================================================
// ЗАВДАННЯ 2: ФОНОВИЙ ІНДИКАТОР (Перевірка багатозадачності)
// ==============================================================================
void task_blinker_main(void) {
    bool is_red = false;
    
    while (1) {
        // Фонова задача більше НЕ малює. Вона просто змінює стан!
        if (is_red) blinker_color = 1;
        else blinker_color = 2;
        
        is_red = !is_red;
        
        // Спокійно засинаємо рівно на 1 секунду.
        sleep(1); 
    }
}

// ==============================================================================
// ЯДРО
// ==============================================================================
void kernel_main(uint32_t magic, multiboot_info_t* mbd) {
    init_gdt(); 
    init_idt();
    init_kheap(0x1000000, 16 * 1024 * 1024);

    if (magic == 0x2BADB002 && (mbd->flags & (1 << 12))) {
        uint32_t fb_addr = (uint32_t)(mbd->framebuffer_addr & 0xFFFFFFFF); 
        screen_w = mbd->framebuffer_width; 
        screen_h = mbd->framebuffer_height; 
        init_graphics((uint32_t*)fb_addr, screen_w, screen_h, mbd->framebuffer_pitch, mbd->framebuffer_bpp);
        
        if (mbd->flags & (1 << 3)) {
            multiboot_module_t* mod = (multiboot_module_t*)mbd->mods_addr;
            if (mbd->mods_count > 0) main_font_data = (uint8_t*)mod[0].mod_start;
            if (mbd->mods_count > 1) { bg_image_data = (uint8_t*)mod[1].mod_start; bg_image_size = mod[1].mod_end - mod[1].mod_start; }
            if (mbd->mods_count > 2) { icon_image_data = (uint8_t*)mod[2].mod_start; icon_image_size = mod[2].mod_end - mod[2].mod_start; }
            if (mbd->mods_count > 3) cache_folder_image((uint8_t*)mod[3].mod_start, mod[3].mod_end - mod[3].mod_start);
            if (mbd->mods_count > 4) cache_file_image((uint8_t*)mod[4].mod_start, mod[4].mod_end - mod[4].mod_start);
        }
    } 

    init_keyboard(); 
    init_mouse(); 
    init_timer(50); 
    init_shell(); 
    init_fs();
    
    if (bg_image_data) cache_background_image(bg_image_data, bg_image_size);
    if (icon_image_data) cache_icon_image(icon_image_data, icon_image_size);

    desktop_init(screen_w, screen_h);

    init_multitasking();
    create_task(task_gui_main); 
    create_task(task_blinker_main); 

    while (1) {
        asm volatile("hlt");
    }
}