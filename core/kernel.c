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
#include "kstring.h"
#include "timer.h"

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
    (void)destlen;
    return memset(dest, ch, count);
}

void* __memcpy_chk(void* dest, const void* src, size_t len, size_t destlen) {
    (void)destlen;
    return memcpy(dest, src, len);
}
// ==============================================================================

void print(const char* str); 
extern void init_timer(uint32_t frequency);
extern void itoa(uint32_t num, char* str);
int custom_strcmp(const char *s1, const char *s2);

// [ВИПРАВЛЕННЯ 1]: Додано прототип, щоб функція print() знала про термінал
static void term_gui_feed(char c);

static uint32_t screen_w = 0;
static uint32_t screen_h = 0;
static uint8_t last_second = 255;

typedef enum {
    STATE_LOGIN,
    STATE_DESKTOP
} os_state_t;

os_state_t current_state = STATE_LOGIN;

// [ВИПРАВЛЕННЯ 2]: Перенесли змінну вище, щоб try_login() могла її змінити
bool win_open = false; 

// --- ЗМІННІ ДЛЯ ЛОГІНУ ---
char username_buffer[32] = "";
int username_len = 0;
char password_buffer[32] = "";
int password_len = 0;
bool login_failed = false;

int active_field = 0; // 0: Username, 1: Password

typedef struct {
    bool authenticated;
    char username[32];
    uint32_t password_shadow_hash;
} user_session_t;

static user_session_t user_session = { false, "", 0 };

static uint32_t shadow_hash_password(const char* pass) {
    // Lightweight kernel-side hash cache (no plaintext persistence after auth).
    uint32_t h = 2166136261u;
    for (size_t i = 0; pass[i] != '\0'; i++) {
        h ^= (uint8_t)pass[i];
        h *= 16777619u;
    }
    return h;
}

static bool point_in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

static void try_login(void) {
    uint32_t in_hash = shadow_hash_password(password_buffer);
    if (custom_strcmp(username_buffer, "admin") == 0 && in_hash == shadow_hash_password("1234")) {
        user_session.authenticated = true;
        memcpy(user_session.username, username_buffer, username_len + 1);
        user_session.password_shadow_hash = in_hash;
        memset(password_buffer, 0, sizeof(password_buffer));
        password_len = 0;
        current_state = STATE_DESKTOP;
        win_open = true;
        login_failed = false;
    } else {
        user_session.authenticated = false;
        user_session.username[0] = '\0';
        user_session.password_shadow_hash = 0;
        login_failed = true;
        password_len = 0;
        password_buffer[0] = '\0';
        username_len = 0;
        username_buffer[0] = '\0';
        active_field = 0;
    }
}

// --- ЗМІННІ РОБОЧОГО СТОЛУ ТА ВІКОН ---
int win_x = 300; int win_y = 200; int win_w = 600; int win_h = 400;
bool is_dragging = false; int drag_offset_x = 0; int drag_offset_y = 0; int selected_file = -1; 
bool viewer_open = false; int viewer_x = 350; int viewer_y = 250; int viewer_w = 500; int viewer_h = 300;
bool viewer_dragging = false; int viewer_drag_x = 0; int viewer_drag_y = 0; int viewer_file_id = -1; 
bool context_menu_open = false; int context_x = 0; int context_y = 0;
bool login_hover_cancel = false;
bool login_hover_login = false;
bool login_pressed_cancel = false;
bool login_pressed_login = false;

/* --- Desktop environment (Kali-style) --- */
#define DESKTOP_TOP_BAR_H   32
#define DESKTOP_DOCK_W      60
#define DESKTOP_TITLE_H     32
#define TERM_LINE_CAP       40
#define TERM_LINE_W         120

static bool term_win_open = true;
static int term_x = 72;
static int term_y = 52;
static int term_w = 540;
static int term_h = 360;
static bool term_dragging = false;
static int term_drag_off_x = 0;
static int term_drag_off_y = 0;

static int fm_nav_sel = 0;
static const char* fm_nav_paths[] = { "/home", "/boot", "/etc", "/var" };
#define FM_NAV_COUNT 4

/* active_win: 0 = Terminal on top, 1 = Files on top */
static int active_win = 1;

static char term_lines[TERM_LINE_CAP][TERM_LINE_W];
static int term_line_head = 0;
static int term_line_count = 0;
static char term_cur_line[TERM_LINE_W];
static int term_cur_len = 0;

extern void shell_handle_keypress(char c);

// --- ДОПОМІЖНІ ФУНКЦІЇ ---
int custom_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void custom_strcpy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static int find_free_vfs_slot(void) {
    for (int i = 0; i < MAX_FILES; i++) if (!virtual_fs[i].is_used) return i;
    return -1;
}

static int decimal_len(int n) {
    int len = 1;
    while (n >= 10) { n /= 10; len++; }
    return len;
}

static void append_uint(char* out, int n) {
    int len = decimal_len(n);
    out[len] = '\0';
    for (int i = len - 1; i >= 0; i--) {
        out[i] = '0' + (n % 10);
        n /= 10;
    }
}

static void vfs_create_named_file(const char* prefix, const char* ext, const char* content) {
    int slot = find_free_vfs_slot();
    if (slot < 0) return;

    char num[8];
    int id = slot + 1;
    append_uint(num, id);

    char name[32];
    int p = 0;
    for (int i = 0; prefix[i] && p < 31; i++) name[p++] = prefix[i];
    for (int i = 0; num[i] && p < 31; i++) name[p++] = num[i];
    for (int i = 0; ext[i] && p < 31; i++) name[p++] = ext[i];
    name[p] = '\0';

    custom_strcpy(virtual_fs[slot].name, name);
    custom_strcpy(virtual_fs[slot].content, content);
    virtual_fs[slot].size = (uint32_t)strlen(content);
    virtual_fs[slot].is_used = true;
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

static void vga_terminal_putchar(char c) {
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

void terminal_putchar(char c) {
    vga_terminal_putchar(c);
}

void print(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (current_state == STATE_DESKTOP && term_win_open) term_gui_feed(str[i]);
        vga_terminal_putchar(str[i]);
    }
}

// --- GUI ФУНКЦІЇ ---
void draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < w; i++) { draw_pixel(x + i, y, color); draw_pixel(x + i, y + h - 1, color); }
    for (int i = 0; i < h; i++) { draw_pixel(x, y + i, color); draw_pixel(x + w - 1, y + i, color); }
}
void draw_filled_rect(int x, int y, int w, int h, uint32_t color) {
    for (int cy = y; cy < y + h; cy++) for (int cx = x; cx < x + w; cx++) draw_pixel(cx, cy, color);
}

static int rtc_weekday_sun0(uint32_t year, uint8_t month, uint8_t day) {
    const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) year--;
    return (int)((year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7);
}

static void format_rtc_datetime_kali(char* out, size_t cap) {
    uint8_t hour, min, sec, d, mo;
    uint32_t y;
    read_rtc(&hour, &min, &sec, &d, &mo, &y);
    if (cap < 28) { out[0] = '\0'; return; }

    static const char* wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* mn[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int wdi = rtc_weekday_sun0(y, mo, d);
    if (wdi < 0) wdi = 0;
    if (wdi > 6) wdi = 6;
    const char* wds = wd[wdi];
    const char* ms = mn[(mo >= 1 && mo <= 12) ? (mo - 1) : 0];

    size_t p = 0;
    while (wds[0] && p + 1 < cap) out[p++] = *wds++;
    if (p + 1 < cap) out[p++] = ' ';
    while (ms[0] && p + 1 < cap) out[p++] = *ms++;
    if (p + 1 < cap) out[p++] = ' ';
    if (d >= 10) {
        if (p + 1 < cap) out[p++] = (char)('0' + (d / 10));
        if (p + 1 < cap) out[p++] = (char)('0' + (d % 10));
    } else {
        if (p + 1 < cap) out[p++] = (char)('0' + d);
    }
    if (p + 1 < cap) out[p++] = ',';
    if (p + 1 < cap) out[p++] = ' ';
    if (p + 1 < cap) out[p++] = (char)('0' + (hour / 10));
    if (p + 1 < cap) out[p++] = (char)('0' + (hour % 10));
    if (p + 1 < cap) out[p++] = ':';
    if (p + 1 < cap) out[p++] = (char)('0' + (min / 10));
    if (p + 1 < cap) out[p++] = (char)('0' + (min % 10));
    out[p] = '\0';
}

static void term_gui_flush_line(void) {
    term_cur_line[term_cur_len] = '\0';
    custom_strcpy(term_lines[term_line_head], term_cur_line);
    term_line_head = (term_line_head + 1) % TERM_LINE_CAP;
    if (term_line_count < TERM_LINE_CAP) term_line_count++;
    else term_line_count = TERM_LINE_CAP;
    term_cur_len = 0;
    term_cur_line[0] = '\0';
}

static void term_gui_feed(char c) {
    if (current_state != STATE_DESKTOP || !term_win_open) return;
    if (c == '\b') {
        if (term_cur_len > 0) {
            term_cur_len--;
            term_cur_line[term_cur_len] = '\0';
        }
        return;
    }
    if (c == '\n' || c == '\r') {
        if (c == '\n') term_gui_flush_line();
        return;
    }
    if (term_cur_len < TERM_LINE_W - 2) {
        term_cur_line[term_cur_len++] = c;
        term_cur_line[term_cur_len] = '\0';
    }
}

static void draw_desktop_dock_blend(uint32_t w, uint32_t h) {
    const uint32_t dock_c = 0x111111;
    const uint8_t alpha = 200;
    for (int y = (int)DESKTOP_TOP_BAR_H; y < (int)h; y++) {
        for (int x = 0; x < DESKTOP_DOCK_W; x++) {
            uint32_t bg = get_pixel(x, y);
            uint8_t br = (uint8_t)(bg >> 16), bg_g = (uint8_t)(bg >> 8), bb = (uint8_t)bg;
            uint8_t fr = (uint8_t)(dock_c >> 16), fg = (uint8_t)(dock_c >> 8), fb = (uint8_t)dock_c;
            uint8_t r = (uint8_t)((fr * alpha + br * (255 - alpha)) >> 8);
            uint8_t g = (uint8_t)((fg * alpha + bg_g * (255 - alpha)) >> 8);
            uint8_t b = (uint8_t)((fb * alpha + bb * (255 - alpha)) >> 8);
            draw_pixel(x, y, (r << 16) | (g << 8) | b);
        }
    }
    draw_filled_rect(DESKTOP_DOCK_W - 2, DESKTOP_TOP_BAR_H + 4, 2, (int)h - DESKTOP_TOP_BAR_H - 4, 0x2A4A66);
}

static void draw_dock_icon_placeholder(int x, int y, uint32_t accent) {
    draw_rounded_rect(x, y, 40, 40, 8, 0x2A2F38);
    draw_rect_outline(x, y, 40, 40, accent);
}

static void draw_status_tray_icons(int right_x, int y0) {
    int x = right_x;
    /* battery */
    draw_filled_rect(x, y0, 18, 10, 0x3A3A3A);
    draw_filled_rect(x + 18, y0 + 2, 3, 6, 0x3A3A3A);
    draw_filled_rect(x + 3, y0 + 2, 10, 6, 0x55AA66);
    x += 28;
    /* wifi arcs */
    draw_pixel(x + 8, y0 + 8, 0xCCCCCC);
    draw_rect_outline(x + 4, y0 + 4, 8, 6, 0xAAAAAA);
    draw_rect_outline(x + 2, y0 + 2, 12, 10, 0x888888);
    x += 28;
    /* volume */
    draw_filled_rect(x, y0 + 4, 4, 8, 0xCCCCCC);
    draw_filled_rect(x + 4, y0 + 2, 10, 12, 0x666666);
}

static void draw_top_bar_kali(uint32_t w) {
    draw_filled_rect(0, 0, w, DESKTOP_TOP_BAR_H, 0x111111);
    const int bl = 22;
    if (main_font_data) {
        draw_ttf_string(14, bl, main_font_data, "Activities", 15.0f, 0xE8E8E8);
        int ix = 110;
        draw_filled_rect(ix, 8, 18, 16, 0x4A90D9);
        draw_filled_rect(ix + 24, 8, 18, 16, 0x2D2D2D);
        draw_filled_rect(ix + 52, 8, 18, 16, 0x3D3D3D);
        char dt[32];
        format_rtc_datetime_kali(dt, sizeof(dt));
        int tw = measure_ttf_text_width(main_font_data, dt, 15.0f);
        draw_ttf_string((int)((w - (uint32_t)tw) / 2), bl, main_font_data, dt, 15.0f, 0xE8E8E8);
        draw_status_tray_icons((int)w - 118, 11);
    }
}

static void draw_kali_window_frame(int x, int y, int ww, int wh, bool active, const char* title) {
    uint32_t border = active ? 0x00A0C8 : 0x3A3A3A;
    draw_filled_rect(x, y, ww, wh, 0x1A1A1A);
    draw_rect_outline(x, y, ww, wh, border);
    uint32_t title_bg = active ? 0x252A30 : 0x1E1E1E;
    draw_filled_rect(x + 1, y + 1, ww - 2, DESKTOP_TITLE_H - 1, title_bg);
    if (active) draw_filled_rect(x + 1, y + 1, ww - 2, 3, 0x00B4D8);
    if (main_font_data) draw_ttf_string(x + 10, 22, main_font_data, title, 14.0f, 0xE0E0E0);
    int cl_x = x + ww - 34;
    draw_filled_rect(cl_x, y + 4, 28, 24, 0xC03030);
    if (main_font_data) draw_ttf_string(cl_x + 8, 20, main_font_data, "X", 13.0f, 0xFFFFFF);
}

static bool vfs_entry_is_folder(int i) {
    if (i < 0 || i >= MAX_FILES || !virtual_fs[i].is_used) return false;
    return custom_strcmp(virtual_fs[i].content, "<folder>") == 0;
}

static void draw_file_manager_window(void) {
    if (!win_open) return;
    bool active = (active_win == 1);
    draw_kali_window_frame(win_x, win_y, win_w, win_h, active, "Files");
    int inner_y = win_y + DESKTOP_TITLE_H;
    int inner_h = win_h - DESKTOP_TITLE_H;
    int left_w = 168;
    draw_filled_rect(win_x + 1, inner_y, left_w, inner_h, 0x2B2B2B);
    int rx = win_x + 1 + left_w;
    int rw = win_w - 2 - left_w;
    draw_filled_rect(rx, inner_y, rw, inner_h, 0xECECEC);

    if (main_font_data) {
        for (int i = 0; i < FM_NAV_COUNT; i++) {
            int ly = inner_y + 12 + i * 28;
            if (i == fm_nav_sel) draw_filled_rect(win_x + 4, ly - 4, left_w - 8, 26, 0x3678C4);
            uint32_t tc = (i == fm_nav_sel) ? 0xFFFFFF : 0xB0B0B0;
            draw_ttf_string(win_x + 12, ly + 14, main_font_data, fm_nav_paths[i], 14.0f, tc);
        }
    }

    int gx = rx + 16;
    int gy = inner_y + 16;
    int col = 0;
    int max_cols = (rw - 32) / 92;
    if (max_cols < 1) max_cols = 1;
    int fx = gx, fy = gy;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!virtual_fs[i].is_used) continue;
        if (selected_file == i) draw_filled_rect(fx - 4, fy - 4, 84, 88, 0xD0E8FF);
        if (vfs_entry_is_folder(i)) {
            draw_filled_rect(fx, fy, 40, 32, 0x5B9FED);
        } else {
            draw_filled_rect(fx, fy, 40, 32, 0xF5F5F5);
            draw_rect_outline(fx, fy, 40, 32, 0xCCCCCC);
        }
        if (main_font_data) draw_ttf_string(fx - 4, fy + 56, main_font_data, virtual_fs[i].name, 11.0f, 0x222222);
        col++;
        if (col >= max_cols) { col = 0; fx = gx; fy += 100; }
        else fx += 92;
    }

    if (context_menu_open) {
        draw_filled_rect(context_x, context_y, 180, 84, 0x111111);
        draw_rect_outline(context_x, context_y, 180, 84, 0x4488FF);
        if (main_font_data) {
            draw_ttf_string(context_x + 8, context_y + 14, main_font_data, "New Folder", 12.0f, 0xCFE8FF);
            draw_ttf_string(context_x + 8, context_y + 38, main_font_data, "New Text File", 12.0f, 0xCFE8FF);
            draw_ttf_string(context_x + 8, context_y + 62, main_font_data, "Delete", 12.0f, (selected_file != -1) ? 0xFF8888 : 0x666666);
        }
    }
}

static void draw_terminal_window(void) {
    if (!term_win_open) return;
    bool active = (active_win == 0);
    draw_kali_window_frame(term_x, term_y, term_w, term_h, active, "Terminal");
    int cy = term_y + DESKTOP_TITLE_H + 8;
    const int line_h = 18;
    const int max_vis = (term_h - DESKTOP_TITLE_H - 16) / line_h;
    int n_show = term_line_count;
    if (n_show > max_vis) n_show = max_vis;
    int start_idx = (term_line_head - n_show + TERM_LINE_CAP * 4) % TERM_LINE_CAP;
    if (main_font_data) {
        for (int r = 0; r < n_show; r++) {
            int idx = (start_idx + r) % TERM_LINE_CAP;
            draw_ttf_string(term_x + 10, cy + 14, main_font_data, term_lines[idx], 13.0f, 0xD0D0D0);
            cy += line_h;
        }
        char prompt_line[SHELL_BUFFER_SIZE + 16];
        int pp = 0;
        const char* pr = "B-nix> ";
        while (pr[0] && pp < (int)sizeof(prompt_line) - 2) prompt_line[pp++] = *pr++;
        for (int i = 0; i < buffer_index && pp < (int)sizeof(prompt_line) - 2; i++) prompt_line[pp++] = command_buffer[i];
        prompt_line[pp] = '\0';
        draw_ttf_string(term_x + 10, cy + 14, main_font_data, prompt_line, 13.0f, 0x88FF88);
    }
}

static bool desktop_hit_fm_nav(int mx, int my, int* out_sel) {
    if (!win_open) return false;
    int inner_y = win_y + DESKTOP_TITLE_H;
    int left_w = 168;
    if (mx < win_x + 4 || mx > win_x + left_w - 4) return false;
    for (int i = 0; i < FM_NAV_COUNT; i++) {
        int ly = inner_y + 12 + i * 28;
        if (my >= ly - 4 && my < ly + 24) {
            *out_sel = i;
            return true;
        }
    }
    return false;
}

static int desktop_pick_grid_file(int mx, int my) {
    if (!win_open) return -1;
    int inner_y = win_y + DESKTOP_TITLE_H;
    int left_w = 168;
    int rx = win_x + 1 + left_w;
    int rw = win_w - 2 - left_w;
    int gx = rx + 16;
    int gy = inner_y + 16;
    int max_cols = (rw - 32) / 92;
    if (max_cols < 1) max_cols = 1;
    int fx = gx, fy = gy, col = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!virtual_fs[i].is_used) continue;
        if (mx >= fx - 4 && mx < fx + 84 && my >= fy - 4 && my < fy + 88) return i;
        col++;
        if (col >= max_cols) {
            col = 0;
            fx = gx;
            fy += 100;
        } else fx += 92;
    }
    return -1;
}

static void draw_desktop_chrome(uint32_t w, uint32_t h) {
    draw_top_bar_kali(w);
    draw_desktop_dock_blend(w, h);
    for (int i = 0; i < 3; i++) {
        uint32_t ac = (i == 0) ? 0x4A90D9 : ((i == 1) ? 0x00A0C8 : 0x666666);
        draw_dock_icon_placeholder(10, DESKTOP_TOP_BAR_H + 14 + i * 58, ac);
    }
}

static void draw_desktop_windows(void) {
    if (active_win == 0) {
        draw_file_manager_window();
        draw_terminal_window();
    } else {
        draw_terminal_window();
        draw_file_manager_window();
    }
}

// --- ЕКРАН ВХОДУ (KALI STYLE + PNG) ---
void draw_login_screen(uint32_t width, uint32_t height) {
    draw_cached_background();
    const int top_h = 38;
    const float top_font = 22.0f;
    const int top_baseline_y = 28;
    draw_filled_rect(0, 0, width, top_h, 0x1A1A1A);
    if (main_font_data) {
        draw_ttf_string(12, top_baseline_y, main_font_data, "B-nix", top_font, 0xBFD3E6);
        uint8_t hour, min, sec, d, mo; uint32_t y; read_rtc(&hour, &min, &sec, &d, &mo, &y);
        char ts[6] = "00:00"; ts[0]=(hour/10)+'0'; ts[1]=(hour%10)+'0'; ts[3]=(min/10)+'0'; ts[4]=(min%10)+'0';
        draw_ttf_string(width - 86, top_baseline_y, main_font_data, ts, top_font, 0xBFD3E6);
    }

    int cx = width / 2;
    int cy = height / 2;

    int win_w = 400; int win_h = 218;
    int wx = cx - win_w / 2;
    int wy = cy - win_h / 2;
    draw_rounded_rect(wx, wy, win_w, win_h, 16, 0x111111);

    draw_cached_icon_centered(cx, wy + 40);

    int field_w = 320; int field_h = 44;
    int field_x = cx - field_w / 2;
    int text_baseline_offset = 30;
    int username_text_px = measure_ttf_text_width(main_font_data, username_buffer, 18.0f);
    int dot_pitch = measure_ttf_text_width(main_font_data, "*", 18.0f);
    if (dot_pitch < 10) dot_pitch = 10;

    int user_y = wy + 34;
    if (active_field == 0) {
        draw_rounded_rect(field_x - 2, user_y - 2, field_w + 4, field_h + 4, 9, 0x1C315A);
    }
    draw_rounded_rect(field_x, user_y, field_w, field_h, 8, 0x0A0A0A);
    if (active_field == 0) {
        draw_rounded_rect(field_x - 1, user_y - 1, field_w + 2, field_h + 2, 8, 0x2A4F90);
        draw_rounded_rect(field_x, user_y, field_w, field_h, 8, 0x0A0A0A);
    }
    if (main_font_data) {
        if (username_len > 0) draw_ttf_string(field_x + 14, user_y + text_baseline_offset, main_font_data, username_buffer, 18.0, 0xFFFFFF);
        else draw_ttf_string(field_x + 14, user_y + text_baseline_offset, main_font_data, "Username", 18.0, 0x666D78);
    }
    if (active_field == 0 && ((timer_ticks / 25) & 1u) == 0u) {
        int caret_x = field_x + 14 + username_text_px + 1;
        draw_filled_rect(caret_x, user_y + 12, 2, 20, 0x88B8FF);
    }

    int pass_y = wy + 86;
    if (active_field == 1) {
        draw_rounded_rect(field_x - 2, pass_y - 2, field_w + 4, field_h + 4, 9, 0x1C315A);
    }
    draw_rounded_rect(field_x, pass_y, field_w, field_h, 8, 0x0A0A0A);
    if (active_field == 1) {
        draw_rounded_rect(field_x - 1, pass_y - 1, field_w + 2, field_h + 2, 8, 0x2A4F90);
        draw_rounded_rect(field_x, pass_y, field_w, field_h, 8, 0x0A0A0A);
    }
    if (main_font_data) {
        if (password_len > 0) {
            for (int i = 0; i < password_len; i++) {
                draw_filled_circle(field_x + 20 + (i * dot_pitch), pass_y + 22, 4, 0xFFFFFF);
            }
        } else {
            draw_ttf_string(field_x + 14, pass_y + text_baseline_offset, main_font_data, "Password", 18.0, 0x666D78);
        }
    }
    if (active_field == 1 && ((timer_ticks / 25) & 1u) == 0u) {
        int caret_x = field_x + 20 + (password_len * dot_pitch) + 1;
        draw_filled_rect(caret_x, pass_y + 12, 2, 20, 0x88B8FF);
    }

    if (login_failed && main_font_data) {
        draw_ttf_string(cx - 100, pass_y + 62, main_font_data, "Authentication failed!", 16.0, 0xFF3333);
    }

    int btn_w = 150; int btn_h = 34;
    int btn_y = wy + 166;

    int cancel_x = cx - 160;
    uint32_t cancel_fill = login_pressed_cancel ? 0x1E1E1E : (login_hover_cancel ? 0x2E2E2E : 0x2A2A2A);
    uint32_t cancel_border = login_pressed_cancel ? 0x222222 : (login_hover_cancel ? 0x4A4A4A : 0x333333);
    draw_rounded_rect(cancel_x, btn_y, btn_w, btn_h, 6, cancel_fill);
    draw_rect_outline(cancel_x, btn_y, btn_w, btn_h, cancel_border);
    if (main_font_data) {
        int tw = measure_ttf_text_width(main_font_data, "Cancel", 16.0f);
        draw_ttf_string(cancel_x + (btn_w - tw) / 2, btn_y + 23, main_font_data, "Cancel", 16.0, 0xD4D8DE);
    }
    else draw_string(cancel_x + 50, btn_y + 10, "Cancel", 0xCCCCCC, 1);

    int login_x = cx + 10;
    uint32_t login_fill = login_pressed_login ? 0x2E57AD : (login_hover_login ? 0x3C72DC : 0x3366CC);
    uint32_t login_border = login_pressed_login ? 0x3A6FD8 : (login_hover_login ? 0x66A4FF : 0x4488FF);
    draw_rounded_rect(login_x, btn_y, btn_w, btn_h, 6, login_fill);
    draw_rect_outline(login_x, btn_y, btn_w, btn_h, login_border);
    if (main_font_data) {
        int tw = measure_ttf_text_width(main_font_data, "Log In", 16.0f);
        draw_ttf_string(login_x + (btn_w - tw) / 2, btn_y + 23, main_font_data, "Log In", 16.0, 0xFFFFFF);
    }
    else draw_string(login_x + 50, btn_y + 10, "Log In", 0xFFFFFF, 1);
}

// --- Desktop viewer (text file) ---
void draw_viewer() {
    if (!viewer_open || viewer_file_id == -1) return;
    draw_kali_window_frame(viewer_x, viewer_y, viewer_w, viewer_h, true, virtual_fs[viewer_file_id].name);
    if (main_font_data)
        draw_ttf_string(viewer_x + 12, viewer_y + DESKTOP_TITLE_H + 20, main_font_data, virtual_fs[viewer_file_id].content, 13.0f, 0xD0D0D0);
    else
        draw_string(viewer_x + 10, viewer_y + 40, virtual_fs[viewer_file_id].content, 0xCCCCCC, 1);
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
    if (bg_image_data) cache_background_image(bg_image_data, bg_image_size);
    if (icon_image_data) cache_icon_image(icon_image_data, icon_image_size);

    bool prev_c = false; bool prev_r = false;
    while (1) {
        int mx = mouse_x;
        int my = mouse_y;
        bool left_now = mouse_left_pressed;
        bool right_now = mouse_right_pressed;
        bool j_c = (left_now && !prev_c);
        bool j_r = (right_now && !prev_r);

        if (current_state == STATE_LOGIN) {
            int cx = (int)screen_w / 2;
            int cy = (int)screen_h / 2;
            int login_w = 400, login_h = 218;
            int login_x = cx - login_w / 2;
            int login_y = cy - login_h / 2;
            int btn_w = 150, btn_h = 34, btn_y = login_y + 166;
            int cancel_x = cx - 160, login_btn_x = cx + 10;

            login_hover_cancel = point_in_rect(mx, my, cancel_x, btn_y, btn_w, btn_h);
            login_hover_login = point_in_rect(mx, my, login_btn_x, btn_y, btn_w, btn_h);
            login_pressed_cancel = left_now && login_hover_cancel;
            login_pressed_login = left_now && login_hover_login;

            if (j_c) {
                if (login_hover_cancel) {
                    username_len = 0; username_buffer[0] = '\0';
                    password_len = 0; password_buffer[0] = '\0';
                    login_failed = false;
                    active_field = 0;
                } else if (login_hover_login) {
                    try_login();
                } else {
                    int field_w = 320, field_h = 44, field_x = cx - field_w / 2;
                    int user_y = login_y + 34;
                    int pass_y = login_y + 86;
                    if (point_in_rect(mx, my, field_x, user_y, field_w, field_h)) active_field = 0;
                    else if (point_in_rect(mx, my, field_x, pass_y, field_w, field_h)) active_field = 1;
                }
            }

            if (key_ready) {
                if (last_key_pressed == '\b') {
                    if (active_field == 0) { if (username_len > 0) username_buffer[--username_len] = '\0'; }
                    else { if (password_len > 0) password_buffer[--password_len] = '\0'; }
                }
                else if (last_key_pressed == '\t') {
                    active_field = (active_field == 0) ? 1 : 0;
                }
                else if (last_key_pressed == '\n') {
                    try_login();
                }
                else {
                    if (active_field == 0) { if (username_len < 30) username_buffer[username_len++] = last_key_pressed; username_buffer[username_len] = '\0'; }
                    else { if (password_len < 30) password_buffer[password_len++] = last_key_pressed; password_buffer[password_len] = '\0'; }
                }
                key_ready = false;
            }
            draw_login_screen(screen_w, screen_h);
        } else {
            draw_cached_background();
            draw_desktop_chrome(screen_w, screen_h);

            if (key_ready) {
                if (active_win == 0 && term_win_open)
                    shell_handle_keypress(last_key_pressed);
                key_ready = false;
            }

            if (j_r && win_open && mx >= win_x && mx <= win_x + win_w && my >= win_y + DESKTOP_TITLE_H && my <= win_y + win_h) {
                context_menu_open = true;
                context_x = mx;
                context_y = my;
            }

            if (j_c && context_menu_open) {
                if (mx >= context_x && mx <= context_x + 180 && my >= context_y && my <= context_y + 84) {
                    int item = (my - context_y) / 28;
                    if (item > 2) item = 2;
                    if (item < 0) item = 0;
                    if (item == 0) vfs_create_named_file("NewFolder", "", "<folder>");
                    else if (item == 1) vfs_create_named_file("NewFile", ".txt", "");
                    else if (item == 2 && selected_file != -1) {
                        virtual_fs[selected_file].is_used = false;
                        selected_file = -1;
                    }
                }
                context_menu_open = false;
            }
            else if (j_c && my >= 0 && my < DESKTOP_TOP_BAR_H) {
            }
            else if (j_c && mx >= 0 && mx < DESKTOP_DOCK_W && my >= DESKTOP_TOP_BAR_H) {
                int di = (my - DESKTOP_TOP_BAR_H) / 58;
                if (di == 0) {
                    term_win_open = true;
                    active_win = 0;
                } else if (di == 1) {
                    win_open = true;
                    active_win = 1;
                }
            }
            else if (viewer_open && j_c) {
                int vcb = viewer_x + viewer_w - 34;
                if (my >= viewer_y && my <= viewer_y + DESKTOP_TITLE_H && mx >= vcb && mx <= viewer_x + viewer_w)
                    viewer_open = false;
                else if (my >= viewer_y && my <= viewer_y + DESKTOP_TITLE_H && mx >= viewer_x && mx < vcb) {
                    viewer_dragging = true;
                    drag_offset_x = mx - viewer_x;
                    drag_offset_y = my - viewer_y;
                }
            }
            else if (j_c && !viewer_dragging &&
                     !(viewer_open && mx >= viewer_x && mx <= viewer_x + viewer_w && my >= viewer_y && my <= viewer_y + viewer_h)) {
                int z0 = (active_win == 0) ? 0 : 1;
                int z1 = (active_win == 0) ? 1 : 0;
                for (int pass = 0; pass < 2; pass++) {
                    int z = (pass == 0) ? z0 : z1;
                    if (z == 0 && term_win_open) {
                        if (mx < term_x || mx > term_x + term_w || my < term_y || my > term_y + term_h) continue;
                        int tcb = term_x + term_w - 34;
                        if (my >= term_y && my <= term_y + DESKTOP_TITLE_H && mx >= tcb && mx <= term_x + term_w) {
                            term_win_open = false;
                            break;
                        }
                        if (my >= term_y && my <= term_y + DESKTOP_TITLE_H && mx >= term_x && mx < tcb) {
                            active_win = 0;
                            term_dragging = true;
                            term_drag_off_x = mx - term_x;
                            term_drag_off_y = my - term_y;
                            break;
                        }
                    }
                    if (z == 1 && win_open) {
                        if (mx < win_x || mx > win_x + win_w || my < win_y || my > win_y + win_h) continue;
                        int fcb = win_x + win_w - 34;
                        if (my >= win_y && my <= win_y + DESKTOP_TITLE_H && mx >= fcb && mx <= win_x + win_w) {
                            win_open = false;
                            selected_file = -1;
                            break;
                        }
                        if (my >= win_y && my <= win_y + DESKTOP_TITLE_H && mx >= win_x && mx < fcb) {
                            active_win = 1;
                            is_dragging = true;
                            drag_offset_x = mx - win_x;
                            drag_offset_y = my - win_y;
                            break;
                        }
                        if (my > win_y + DESKTOP_TITLE_H) {
                            int nav_sel_hit = fm_nav_sel;
                            if (desktop_hit_fm_nav(mx, my, &nav_sel_hit)) fm_nav_sel = nav_sel_hit;
                            else {
                                int g = desktop_pick_grid_file(mx, my);
                                if (g >= 0) {
                                    if (selected_file == g && !vfs_entry_is_folder(g)) {
                                        viewer_open = true;
                                        viewer_file_id = g;
                                    } else selected_file = g;
                                } else if (my < win_y + win_h - 10) selected_file = -1;
                            }
                            break;
                        }
                    }
                }
            }
            if (viewer_dragging) {
                if (left_now) {
                    viewer_x = mx - drag_offset_x;
                    viewer_y = my - drag_offset_y;
                    if (viewer_y < DESKTOP_TOP_BAR_H) viewer_y = DESKTOP_TOP_BAR_H;
                } else viewer_dragging = false;
            }

            if (term_dragging) {
                if (left_now) {
                    term_x = mx - term_drag_off_x;
                    term_y = my - term_drag_off_y;
                    if (term_x < DESKTOP_DOCK_W) term_x = DESKTOP_DOCK_W;
                    if (term_y < DESKTOP_TOP_BAR_H) term_y = DESKTOP_TOP_BAR_H;
                    if (term_x + term_w > (int)screen_w) term_x = (int)screen_w - term_w;
                    if (term_y + term_h > (int)screen_h) term_y = (int)screen_h - term_h;
                } else term_dragging = false;
            }
            if (is_dragging) {
                if (left_now) {
                    win_x = mx - drag_offset_x;
                    win_y = my - drag_offset_y;
                    if (win_x < DESKTOP_DOCK_W) win_x = DESKTOP_DOCK_W;
                    if (win_y < DESKTOP_TOP_BAR_H) win_y = DESKTOP_TOP_BAR_H;
                    if (win_x + win_w > (int)screen_w) win_x = (int)screen_w - win_w;
                    if (win_y + win_h > (int)screen_h) win_y = (int)screen_h - win_h;
                } else is_dragging = false;
            }

            draw_desktop_windows();
            draw_viewer();
        }
        
        draw_cursor(mx, my); 
        swap_buffers();
        prev_c = left_now;
        prev_r = right_now;
        asm volatile("hlt");
    }
}
