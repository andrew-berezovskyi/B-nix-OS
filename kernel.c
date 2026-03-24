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

// Глобальна адреса шрифту та картинок
uint8_t* main_font_data = NULL;
uint8_t* bg_image_data = NULL;
uint32_t bg_image_size = 0;
uint8_t* icon_image_data = NULL;
uint32_t icon_image_size = 0;

// ==============================================================================
// БАЗОВІ ФУНКЦІЇ ПАМ'ЯТІ
// ==============================================================================
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
    return memset(dest, count, ch);
}
// ==============================================================================

void print(const char* str); 
extern void init_timer(uint32_t frequency);
extern void itoa(uint32_t num, char* str);

static uint32_t screen_w = 0;
static uint32_t screen_h = 0;
static uint8_t last_second = 255;

typedef enum {
    STATE_LOGIN,
    STATE_DESKTOP
} os_state_t;

os_state_t current_state = STATE_LOGIN;

// --- ЗМІННІ ДЛЯ ЛОГІНУ ---
char username_buffer[32] = "";
int username_len = 0;
char password_buffer[32] = "";
int password_len = 0;
bool login_failed = false;

int active_field = 0; // 0: Username, 1: Password

// --- ЗМІННІ РОБОЧОГО СТОЛУ ТА ВІКОН ---
bool win_open = false; int win_x = 300; int win_y = 200; int win_w = 600; int win_h = 400;
bool is_dragging = false; int drag_offset_x = 0; int drag_offset_y = 0; int selected_file = -1; 
bool viewer_open = false; int viewer_x = 350; int viewer_y = 250; int viewer_w = 500; int viewer_h = 300;
bool viewer_dragging = false; int viewer_drag_x = 0; int viewer_drag_y = 0; int viewer_file_id = -1; 
char search_buffer[32] = ""; int search_len = 0;

// --- ДОПОМІЖНІ ФУНКЦІЇ ---
int custom_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void print_hex(uint32_t num) {
    char hex_str[11]; hex_str[0] = '0'; hex_str[1] = 'x'; hex_str[10] = '\0';
    char hex_chars[] = "0123456789ABCDEF";
    for (int i = 9; i >= 2; i--) { hex_str[i] = hex_chars[num & 0xF]; num >>= 4; }
    print(hex_str);
}

static inline uint8_t inb_port(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
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

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) terminal_row = 0;
        return;
    }
    terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = (uint16_t)c | ((uint16_t)terminal_color << 8);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) terminal_row = 0;
    }
}

void print(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) terminal_putchar(str[i]);
}

// --- GUI ФУНКЦІЇ ---
void draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < w; i++) { draw_pixel(x + i, y, color); draw_pixel(x + i, y + h - 1, color); }
    for (int i = 0; i < h; i++) { draw_pixel(x, y + i, color); draw_pixel(x + w - 1, y + i, color); }
}
void draw_filled_rect(int x, int y, int w, int h, uint32_t color) {
    for (int cy = y; cy < y + h; cy++) for (int cx = x; cx < x + w; cx++) draw_pixel(cx, cy, color);
}

// --- ЕКРАН ВХОДУ (KALI STYLE + PNG) ---
void draw_login_screen(uint32_t width, uint32_t height) {
    if (bg_image_data) {
        draw_background_image(bg_image_data, bg_image_size);
    } else {
        clear_screen(0x000000);
    }
    
    draw_filled_rect(0, 0, width, 28, 0x111111);
    draw_rect_outline(0, 0, width, 28, 0x222222);
    if (main_font_data) {
        draw_ttf_string(10, 6, main_font_data, "B-nix OS 2026", 16.0, 0xCCCCCC);
        uint8_t hour, min, sec, d, mo; uint32_t y; read_rtc(&hour, &min, &sec, &d, &mo, &y);
        char ts[6] = "00:00"; ts[0]=(hour/10)+'0'; ts[1]=(hour%10)+'0'; ts[3]=(min/10)+'0'; ts[4]=(min%10)+'0';
        draw_ttf_string(width - 70, 6, main_font_data, ts, 16.0, 0xCCCCCC);
    }

    int cx = width / 2;
    int cy = height / 2;

    int win_w = 400; int win_h = 320;
    int wx = cx - win_w / 2;
    int wy = cy - win_h / 2;
    draw_rounded_rect(wx, wy, win_w, win_h, 16, 0x111111);
    draw_rect_outline(wx, wy, win_w, win_h, 0x222222);

    if (icon_image_data) {
        draw_image_with_alpha(icon_image_data, icon_image_size, cx - 32, wy + 20);
    }
    
    if (main_font_data) {
        draw_ttf_string(cx - 30, wy + 90, main_font_data, "B-nix", 20.0, 0xFFFFFF);
    }

    int field_w = 320; int field_h = 44;
    int field_x = cx - field_w / 2;

    int user_y = wy + 130;
    draw_rounded_rect(field_x, user_y, field_w, field_h, 8, 0x0A0A0A);
    draw_rect_outline(field_x, user_y, field_w, field_h, (active_field == 0) ? 0x4488FF : 0x222222);
    if (main_font_data) {
        if (username_len > 0) draw_ttf_string(field_x + 10, user_y + 12, main_font_data, username_buffer, 18.0, 0xFFFFFF);
        else draw_ttf_string(field_x + 10, user_y + 12, main_font_data, "Username", 18.0, 0x444444);
    }

    int pass_y = wy + 190;
    draw_rounded_rect(field_x, pass_y, field_w, field_h, 8, 0x0A0A0A);
    draw_rect_outline(field_x, pass_y, field_w, field_h, (active_field == 1) ? 0x4488FF : 0x222222);
    if (main_font_data) {
        if (password_len > 0) {
            for(int i=0; i<password_len; i++) draw_filled_circle(field_x + 20 + (i*18), pass_y + 22, 5, 0xFFFFFF);
        } else {
            draw_ttf_string(field_x + 10, pass_y + 12, main_font_data, "Password", 18.0, 0x444444);
        }
    }

    if (login_failed && main_font_data) {
        draw_ttf_string(cx - 100, pass_y + 55, main_font_data, "Authentication failed!", 16.0, 0xFF3333);
    }

    int btn_w = 150; int btn_h = 36;
    int btn_y = wy + win_h - 50;

    int cancel_x = cx - 160;
    draw_rounded_rect(cancel_x, btn_y, btn_w, btn_h, 6, 0x2A2A2A);
    draw_rect_outline(cancel_x, btn_y, btn_w, btn_h, 0x333333);
    draw_string(cancel_x + 50, btn_y + 10, "Cancel", 0xCCCCCC, 1);

    int login_x = cx + 10;
    draw_rounded_rect(login_x, btn_y, btn_w, btn_h, 6, 0x3366CC);
    draw_rect_outline(login_x, btn_y, btn_w, btn_h, 0x4488FF);
    draw_string(login_x + 50, btn_y + 10, "Log In", 0xFFFFFF, 1);
}

// --- РОБОЧИЙ СТІЛ ---
void draw_window() {
    if (!win_open) return;
    draw_filled_rect(win_x, win_y, win_w, win_h, 0x111111);
    draw_rect_outline(win_x, win_y, win_w, win_h, 0x555555);
    draw_filled_rect(win_x + 1, win_y + 1, win_w - 2, 28, 0x2A2A2A);
    draw_string(win_x + 10, win_y + 7, "File Explorer", 0xFFFFFF, 1);
    int cl_x = win_x + win_w - 30;
    draw_filled_rect(cl_x, win_y + 1, 29, 28, 0xAA3333); 
    draw_string(cl_x + 11, win_y + 7, "X", 0xFFFFFF, 1);
    draw_filled_rect(win_x + 10, win_y + win_h - 35, 100, 25, 0x333333);
    draw_string(win_x + 30, win_y + win_h - 28, "DELETE", (selected_file != -1) ? 0xFF5555 : 0x555555, 1);
    
    int fx = win_x + 30; int fy = win_y + 50;
    for (int i = 0; i < MAX_FILES; i++) {
        if (virtual_fs[i].is_used) {
            if (i == selected_file) draw_filled_rect(fx - 5, fy - 5, 80, 70, 0x224466);
            draw_filled_rect(fx, fy, 32, 40, 0xDDDDDD); 
            draw_string(fx - 10, fy + 45, virtual_fs[i].name, 0xFFFFFF, 1);
            fx += 100; if (fx > win_x + win_w - 80) { fx = win_x + 30; fy += 80; }
        }
    }
}

void draw_viewer() {
    if (!viewer_open || viewer_file_id == -1) return;
    draw_filled_rect(viewer_x, viewer_y, viewer_w, viewer_h, 0x1A1A1A);
    draw_rect_outline(viewer_x, viewer_y, viewer_w, viewer_h, 0x555555);
    draw_filled_rect(viewer_x + 1, viewer_y + 1, viewer_w - 2, 28, 0x2A2A55); 
    draw_string(viewer_x + 10, viewer_y + 7, virtual_fs[viewer_file_id].name, 0xFFFFFF, 1);
    draw_string(viewer_x + 10, viewer_y + 40, virtual_fs[viewer_file_id].content, 0xCCCCCC, 1);
}

void draw_start_menu(uint32_t w, uint32_t h) { 
    int mw = 300; int mh = 420; int mx = (w / 2) - 150; int my = h - 465;
    draw_filled_rect(mx, my, mw, mh, 0x111111); 
    draw_rect_outline(mx, my, mw, mh, 0x333333);
    draw_filled_rect(mx + 15, my + 15, 270, 30, 0x222222); 
    if (search_len > 0) draw_string(mx + 25, my + 22, search_buffer, 0xFFFFFF, 1);
    else draw_string(mx + 25, my + 22, "Type to search...", 0x888888, 1);
    draw_rect_outline(mx + 15, my + 90, 40, 40, 0x555555); 
    draw_string(mx + 23, my + 102, "DIR", 0xFFFFFF, 1);
}

void draw_desktop(uint32_t w, uint32_t h) { 
    if (main_font_data) draw_ttf_string((w/2)-120, (h/2)-40, main_font_data, "B-nix OS", 64.0, 0x111111);
    for (int x = 0; x < w; x++) draw_pixel(x, h - 40, 0xFFFFFF); 
    draw_string((w / 2) - 40, h - 36, "B-nix", 0xFFFFFF, 2); 
}

void update_clock(uint32_t w, uint32_t h) { 
    uint8_t hour, min, sec, d, mo; uint32_t y; read_rtc(&hour, &min, &sec, &d, &mo, &y); 
    char ts[6] = "00:00"; ts[0] = (hour/10)+'0'; ts[1] = (hour%10)+'0'; ts[3] = (min/10)+'0'; ts[4] = (min%10)+'0';
    draw_string(w - 90, h - 32, ts, 0xFFFFFF, 1);
}

// --- ГОЛОВНА ФУНКЦІЯ ---
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
        
        // РОЗУМНИЙ ПОШУК ФАЙЛІВ (За їхнім розміром)
        if (mbd->flags & (1 << 3)) {
            multiboot_module_t* mod = (multiboot_module_t*)mbd->mods_addr;
            uint32_t mod_count = mbd->mods_count;

            if (mod_count >= 1) {
                main_font_data = (uint8_t*)mod[0].mod_start; // Шрифт завжди перший
            }
            if (mod_count >= 3) {
                uint32_t size1 = mod[1].mod_end - mod[1].mod_start;
                uint32_t size2 = mod[2].mod_end - mod[2].mod_start;
                
                // Шпалери на 1280x720 завжди важать набагато більше за маленьку іконку.
                if (size1 > size2) {
                    bg_image_data = (uint8_t*)mod[1].mod_start;
                    bg_image_size = size1;
                    icon_image_data = (uint8_t*)mod[2].mod_start;
                    icon_image_size = size2;
                } else {
                    bg_image_data = (uint8_t*)mod[2].mod_start;
                    bg_image_size = size2;
                    icon_image_data = (uint8_t*)mod[1].mod_start;
                    icon_image_size = size1;
                }
            }
        }
    } 

    init_keyboard(); 
    init_mouse(); 
    init_timer(100); 
    init_shell(); 
    init_vfs();

    bool prev_c = false; bool show_m = false;
    while (1) {
        bool j_c = (mouse_left_pressed && !prev_c);

        if (current_state == STATE_LOGIN) {
            if (key_ready) {
                if (last_key_pressed == '\b') {
                    if (active_field == 0) { if (username_len > 0) username_buffer[--username_len] = '\0'; }
                    else { if (password_len > 0) password_buffer[--password_len] = '\0'; }
                }
                else if (last_key_pressed == '\t' || last_key_pressed == '\n') {
                    active_field = (active_field == 0) ? 1 : 0;
                }
                else if (last_key_pressed == ' ') {
                    if (custom_strcmp(username_buffer, "admin") == 0 && custom_strcmp(password_buffer, "1234") == 0) { 
                        current_state = STATE_DESKTOP; login_failed = false; 
                    } else { 
                        login_failed = true; password_len = 0; password_buffer[0] = '\0'; username_len = 0; username_buffer[0] = '\0'; active_field = 0; 
                    }
                }
                else {
                    if (active_field == 0) { if (username_len < 30) username_buffer[username_len++] = last_key_pressed; username_buffer[username_len] = '\0'; }
                    else { if (password_len < 30) password_buffer[password_len++] = last_key_pressed; password_buffer[password_len] = '\0'; }
                }
                key_ready = false;
            }
            draw_login_screen(screen_w, screen_h);
        } else {
            clear_screen(0x000000); 
            draw_desktop(screen_w, screen_h); 
            update_clock(screen_w, screen_h);
            
            if (key_ready) {
                if (show_m) {
                    if (last_key_pressed == '\b') { if (search_len > 0) search_buffer[--search_len] = '\0'; }
                    else if (search_len < 30) { search_buffer[search_len++] = last_key_pressed; search_buffer[search_len] = '\0'; }
                }
                key_ready = false;
            }

            if (viewer_open) {
                int cb_x = viewer_x + viewer_w - 30;
                if (j_c && mouse_y >= viewer_y && mouse_y <= viewer_y + 28 && mouse_x >= cb_x && mouse_x <= viewer_x + viewer_w) {
                    viewer_open = false; 
                }
                else if (j_c && mouse_y >= viewer_y && mouse_y <= viewer_y + 28 && mouse_x >= viewer_x && mouse_x <= viewer_x + viewer_w) { 
                    viewer_dragging = true; drag_offset_x = mouse_x - viewer_x; drag_offset_y = mouse_y - viewer_y; 
                }
            }
            if (viewer_dragging) { 
                if (mouse_left_pressed) { viewer_x = mouse_x - drag_offset_x; viewer_y = mouse_y - drag_offset_y; } 
                else { viewer_dragging = false; } 
            }

            if (win_open && !viewer_dragging && !(viewer_open && mouse_x >= viewer_x && mouse_x <= viewer_x + viewer_w && mouse_y >= viewer_y && mouse_y <= viewer_y + viewer_h)) {
                int cl_x = win_x + win_w - 30;
                if (j_c && mouse_y >= win_y && mouse_y <= win_y + 28 && mouse_x >= cl_x && mouse_x <= win_x + win_w) { 
                    win_open = false; selected_file = -1; 
                }
                else if (j_c && mouse_y >= win_y && mouse_y <= win_y + 28 && mouse_x >= win_x && mouse_x <= win_x + win_w) { 
                    is_dragging = true; drag_offset_x = mouse_x - win_x; drag_offset_y = mouse_y - win_y; 
                }
                else if (j_c && selected_file != -1 && mouse_x >= win_x + 10 && mouse_x <= win_x + 110 && mouse_y >= win_y + win_h - 35 && mouse_y <= win_y + win_h - 10) { 
                    virtual_fs[selected_file].is_used = false; selected_file = -1; 
                }
                else if (j_c) {
                    int fx = win_x + 30; int fy = win_y + 50; bool clicked_on_file = false;
                    for (int i = 0; i < MAX_FILES; i++) {
                        if (virtual_fs[i].is_used) {
                            if (mouse_x >= fx - 10 && mouse_x <= fx + 70 && mouse_y >= fy - 10 && mouse_y <= fy + 60) {
                                if (selected_file == i) { viewer_open = true; viewer_file_id = i; } else { selected_file = i; }
                                clicked_on_file = true; break;
                            }
                            fx += 100; if (fx > win_x + win_w - 80) { fx = win_x + 30; fy += 80; }
                        }
                    }
                    if (!clicked_on_file && mouse_y > win_y + 28 && mouse_y < win_y + win_h - 40) selected_file = -1;
                }
            }
            if (is_dragging) { 
                if (mouse_left_pressed) { win_x = mouse_x - drag_offset_x; win_y = mouse_y - drag_offset_y; } 
                else { is_dragging = false; } 
            }

            int bx = (screen_w/2)-45; int by = screen_h-38;
            bool hov = (mouse_x >= bx && mouse_x <= bx+90 && mouse_y >= by && mouse_y <= by+36);
            if (hov) draw_rect_outline(bx, by, 90, 36, 0x555555);
            if (j_c && hov) show_m = !show_m;
            if (show_m) draw_start_menu(screen_w, screen_h);
            
            draw_window(); 
            draw_viewer();
        }
        
        draw_cursor(mouse_x, mouse_y); 
        swap_buffers();
        prev_c = mouse_left_pressed;
        asm volatile("hlt");
    }
}