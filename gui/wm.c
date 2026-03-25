#include "gui.h"
#include "vbe.h"
#include "fs.h"
#include "shell.h"
#include "ata.h"

#define DESKTOP_TOP_BAR_H   32
#define DESKTOP_DOCK_W      60
#define DESKTOP_TITLE_H     32

window_t windows[MAX_WINDOWS];
int focused_window = -1; // Індекс вікна, яке зараз "зверху" (-1 = робочий стіл порожній)
bool is_dragging = false;
int dragging_window = -1;
int drag_off_x = 0; int drag_off_y = 0;

// --- ДОПОМІЖНІ ФУНКЦІЇ ---
extern size_t strlen(const char* str); // Щоб не було попереджень
static int custom_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
static void custom_strcpy(char* dst, const char* src) {
    while (*src) *dst++ = *src++; *dst = '\0';
}
static int decimal_len(int n) {
    int len = 1; while (n >= 10) { n /= 10; len++; } return len;
}
static void append_uint(char* out, int n) {
    int len = decimal_len(n); out[len] = '\0';
    for (int i = len - 1; i >= 0; i--) { out[i] = '0' + (n % 10); n /= 10; }
}
static int find_free_vfs_slot(void) {
    for (int i = 0; i < MAX_FILES; i++) if (!virtual_fs[i].is_used) return i;
    return -1;
}
static void vfs_create_named_file(const char* prefix, const char* ext, const char* content) {
    int slot = find_free_vfs_slot(); if (slot < 0) return;
    char num[8]; int id = slot + 1; append_uint(num, id);
    char name[32]; int p = 0;
    for (int i = 0; prefix[i] && p < 31; i++) name[p++] = prefix[i];
    for (int i = 0; num[i] && p < 31; i++) name[p++] = num[i];
    for (int i = 0; ext[i] && p < 31; i++) name[p++] = ext[i];
    name[p] = '\0';
    custom_strcpy(virtual_fs[slot].name, name);
    custom_strcpy(virtual_fs[slot].content, content);
    virtual_fs[slot].size = (uint32_t)strlen(content);
    virtual_fs[slot].is_used = true;
    vfs_save_to_disk(); // Одразу зберігаємо
}

// ==============================================================================
// ПРОГРАМА 1: ТЕРМІНАЛ (Індекс 0)
// ==============================================================================
#define TERM_LINE_CAP 40
#define TERM_LINE_W 120
static char term_lines[TERM_LINE_CAP][TERM_LINE_W];
static int term_line_head = 0; static int term_line_count = 0;
static char term_cur_line[TERM_LINE_W]; static int term_cur_len = 0;

static void term_gui_flush_line(void) {
    term_cur_line[term_cur_len] = '\0'; custom_strcpy(term_lines[term_line_head], term_cur_line);
    term_line_head = (term_line_head + 1) % TERM_LINE_CAP;
    if (term_line_count < TERM_LINE_CAP) term_line_count++; else term_line_count = TERM_LINE_CAP;
    term_cur_len = 0; term_cur_line[0] = '\0';
}
void term_gui_feed(char c) {
    if (current_state != STATE_DESKTOP || !windows[0].is_open) return;
    if (c == '\b') { if (term_cur_len > 0) { term_cur_len--; term_cur_line[term_cur_len] = '\0'; } return; }
    if (c == '\n' || c == '\r') { if (c == '\n') term_gui_flush_line(); return; }
    if (term_cur_len < TERM_LINE_W - 2) { term_cur_line[term_cur_len++] = c; term_cur_line[term_cur_len] = '\0'; }
}
void app_term_draw(int x, int y, int w, int h) {
    int cy = y + 8; const int line_h = 18;
    const int max_vis = (h - 16) / line_h;
    int n_show = term_line_count; if (n_show > max_vis) n_show = max_vis;
    int start_idx = (term_line_head - n_show + TERM_LINE_CAP * 4) % TERM_LINE_CAP;
    if (main_font_data) {
        for (int r = 0; r < n_show; r++) {
            int idx = (start_idx + r) % TERM_LINE_CAP;
            draw_ttf_string(x + 10, cy + 14, main_font_data, term_lines[idx], 13.0f, 0xD0D0D0); cy += line_h;
        }
        char prompt_line[SHELL_BUFFER_SIZE + 16]; int pp = 0; const char* pr = "B-nix> ";
        while (pr[0] && pp < (int)sizeof(prompt_line) - 2) prompt_line[pp++] = *pr++;
        for (int i = 0; i < buffer_index && pp < (int)sizeof(prompt_line) - 2; i++) prompt_line[pp++] = command_buffer[i];
        prompt_line[pp] = '\0';
        draw_ttf_string(x + 10, cy + 14, main_font_data, prompt_line, 13.0f, 0x88FF88);
    }
}
void app_term_key(char c) { shell_handle_keypress(c); }

// ==============================================================================
// ПРОГРАМА 2: ПРОВІДНИК (Індекс 1)
// ==============================================================================
static int fm_nav_sel = 0;
static const char* fm_nav_paths[] = { "/home", "/boot", "/etc", "/var" };
#define FM_NAV_COUNT 4
static int selected_file = -1;
static bool context_menu_open = false; static int context_x = 0; static int context_y = 0;
static int viewer_file_id = -1; // Для читалки

static bool vfs_entry_is_folder(int i) {
    if (i < 0 || i >= MAX_FILES || !virtual_fs[i].is_used) return false;
    return custom_strcmp(virtual_fs[i].content, "<folder>") == 0;
}
void app_fm_draw(int x, int y, int w, int h) {
    int left_w = 168;
    draw_filled_rect(x, y, left_w, h, 0x2B2B2B);
    draw_filled_rect(x + left_w, y, w - left_w, h, 0xECECEC);

    if (main_font_data) {
        for (int i = 0; i < 4; i++) {
            int ly = y + 12 + i * 28;
            if (i == fm_nav_sel) draw_filled_rect(x + 4, ly - 4, left_w - 8, 26, 0x3678C4);
            uint32_t tc = (i == fm_nav_sel) ? 0xFFFFFF : 0xB0B0B0;
            draw_ttf_string(x + 12, ly + 14, main_font_data, fm_nav_paths[i], 14.0f, tc);
        }
    }

    int gx = x + left_w + 16; int gy = y + 16; int col = 0;
    int max_cols = (w - left_w - 32) / 92; if (max_cols < 1) max_cols = 1;
    int fx = gx, fy = gy;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!virtual_fs[i].is_used) continue;
        if (selected_file == i) draw_filled_rect(fx - 4, fy - 4, 84, 88, 0xD0E8FF);
        if (vfs_entry_is_folder(i)) { 
            draw_icon_folder(fx, fy); 
        } else { 
            draw_icon_file(fx + 4, fy - 8); // +4 та -8 щоб відцентрувати файл відносно папки
        }
        if (main_font_data) draw_ttf_string(fx - 4, fy + 56, main_font_data, virtual_fs[i].name, 11.0f, 0x222222);
        col++; if (col >= max_cols) { col = 0; fx = gx; fy += 100; } else fx += 92;
    }

    if (context_menu_open) {
        draw_filled_rect(context_x, context_y, 180, 84, 0x111111); draw_rect_outline(context_x, context_y, 180, 84, 0x4488FF);
        if (main_font_data) {
            draw_ttf_string(context_x + 8, context_y + 14, main_font_data, "New Folder", 12.0f, 0xCFE8FF);
            draw_ttf_string(context_x + 8, context_y + 38, main_font_data, "New Text File", 12.0f, 0xCFE8FF);
            draw_ttf_string(context_x + 8, context_y + 62, main_font_data, "Delete", 12.0f, (selected_file != -1) ? 0xFF8888 : 0x666666);
        }
    }
}
void app_fm_click(int mx, int my, bool right_click) {
    if (right_click) { context_menu_open = true; context_x = mx; context_y = my; return; }
    
    if (context_menu_open) {
        if (mx >= context_x && mx <= context_x + 180 && my >= context_y && my <= context_y + 84) {
            int item = (my - context_y) / 28; if (item > 2) item = 2; if (item < 0) item = 0;
            if (item == 0) vfs_create_named_file("NewFolder", "", "<folder>"); 
            else if (item == 1) vfs_create_named_file("NewFile", ".txt", "");
            else if (item == 2 && selected_file != -1) { virtual_fs[selected_file].is_used = false; selected_file = -1; vfs_save_to_disk(); }
        }
        context_menu_open = false; return;
    }

    int win_idx = 1; // Індекс провідника
    int wx = windows[win_idx].x; int wy = windows[win_idx].y;
    int left_w = 168; int inner_y = wy + DESKTOP_TITLE_H;
    
    // 1. Клік по лівому меню (Sidebar)
    if (mx >= wx && mx < wx + left_w) {
        for (int i = 0; i < FM_NAV_COUNT; i++) {
            int ly = inner_y + 12 + i * 28;
            if (my >= ly - 4 && my < ly + 24) { fm_nav_sel = i; selected_file = -1; return; }
        }
        return;
    }

    // 2. Клік по файлах
    int gx = wx + left_w + 16; int gy = inner_y + 16;
    int max_cols = (windows[win_idx].width - left_w - 32) / 92; if (max_cols < 1) max_cols = 1;
    
    int fx = gx, fy = gy, col = 0; bool hit_file = false;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!virtual_fs[i].is_used) continue;
        if (mx >= fx - 4 && mx < fx + 84 && my >= fy - 4 && my < fy + 88) {
            hit_file = true;
            // Якщо файл вже був вибраний (подвійний клік) - відкриваємо його
            if (selected_file == i && !vfs_entry_is_folder(i)) {
                viewer_file_id = i;
                windows[2].is_open = true; // Відкриваємо вікно Viewer
                custom_strcpy(windows[2].title, virtual_fs[i].name); // Міняємо заголовок вікна на ім'я файлу
                focused_window = 2; // Фокусуємо на ньому
            } else {
                selected_file = i;
            }
            break;
        }
        col++; if (col >= max_cols) { col = 0; fx = gx; fy += 100; } else fx += 92;
    }
    if (!hit_file) selected_file = -1;
}

// ==============================================================================
// ПРОГРАМА 3: ПЕРЕГЛЯДАЧ ФАЙЛІВ (Індекс 2)
// ==============================================================================
void app_viewer_draw(int x, int y, int w, int h) {
    if (viewer_file_id == -1) return;
    if (main_font_data) {
        draw_ttf_string(x + 12, y + 16, main_font_data, virtual_fs[viewer_file_id].content, 13.0f, 0xD0D0D0);
    }
}

// ==============================================================================
// УНІВЕРСАЛЬНИЙ WINDOW MANAGER (ДВИГУН)
// ==============================================================================
void wm_init(void) {
    for(int i=0; i<MAX_WINDOWS; i++) windows[i].is_open = false;

    // Реєструємо Термінал (Індекс 0)
    windows[0].is_open = false;
    windows[0].x = 72; windows[0].y = 52; windows[0].width = 540; windows[0].height = 360;
    custom_strcpy(windows[0].title, "Terminal");
    windows[0].draw_content = app_term_draw;
    windows[0].on_keypress = app_term_key;
    windows[0].on_click = NULL;

    // Реєструємо Провідник (Індекс 1)
    windows[1].is_open = false;
    windows[1].x = 300; windows[1].y = 200; windows[1].width = 600; windows[1].height = 400;
    custom_strcpy(windows[1].title, "Files");
    windows[1].draw_content = app_fm_draw;
    windows[1].on_keypress = NULL;
    windows[1].on_click = app_fm_click;
    
    // Реєструємо Переглядач тексту (Індекс 2)
    windows[2].is_open = false;
    windows[2].x = 400; windows[2].y = 150; windows[2].width = 450; windows[2].height = 300;
    custom_strcpy(windows[2].title, "Text Viewer");
    windows[2].draw_content = app_viewer_draw;
    windows[2].on_keypress = NULL;
    windows[2].on_click = NULL;

    focused_window = -1; // Робочий стіл порожній при старті
}

void wm_draw_windows(void) {
    // 1. Спочатку малюємо всі неактивні вікна (задній план)
    for(int i=0; i<MAX_WINDOWS; i++) {
        if(windows[i].is_open && i != focused_window) {
            draw_kali_window_frame(windows[i].x, windows[i].y, windows[i].width, windows[i].height, false, windows[i].title);
            if(windows[i].draw_content) windows[i].draw_content(windows[i].x + 1, windows[i].y + DESKTOP_TITLE_H, windows[i].width - 2, windows[i].height - DESKTOP_TITLE_H - 1);
        }
    }
    // 2. Потім малюємо активне вікно поверх усіх (передній план)
    if (focused_window >= 0 && windows[focused_window].is_open) {
        int i = focused_window;
        draw_kali_window_frame(windows[i].x, windows[i].y, windows[i].width, windows[i].height, true, windows[i].title);
        if(windows[i].draw_content) windows[i].draw_content(windows[i].x + 1, windows[i].y + DESKTOP_TITLE_H, windows[i].width - 2, windows[i].height - DESKTOP_TITLE_H - 1);
    }
}

void wm_handle_keypress(char c) {
    if (focused_window >= 0 && windows[focused_window].is_open && windows[focused_window].on_keypress) {
        windows[focused_window].on_keypress(c);
    }
}

void wm_process_mouse(int mx, int my, bool left_now, bool right_now, bool j_c, bool j_r) {
    // 1. Обробка перетягування вікна
    if (is_dragging && dragging_window != -1) {
        if (left_now) {
            windows[dragging_window].x = mx - drag_off_x;
            windows[dragging_window].y = my - drag_off_y;
            if(windows[dragging_window].y < DESKTOP_TOP_BAR_H) windows[dragging_window].y = DESKTOP_TOP_BAR_H;
        } else {
            is_dragging = false; dragging_window = -1;
        }
        return;
    }

    if (!j_c && !j_r) return; // Якщо нічого не клікнули - виходимо

    // 2. Обробка кліку по іконках у Док-панелі зліва
    if (mx < DESKTOP_DOCK_W && my >= DESKTOP_TOP_BAR_H) {
        int di = (my - DESKTOP_TOP_BAR_H) / 58;
        if (di == 0) { windows[0].is_open = true; focused_window = 0; } // Відкрити термінал
        if (di == 1) { windows[1].is_open = true; focused_window = 1; } // Відкрити провідник
        return;
    }

    // 3. Визначаємо, по якому вікну клікнули (перевіряємо спочатку верхнє)
    int win_hit = -1;
    if (focused_window != -1 && windows[focused_window].is_open &&
        mx >= windows[focused_window].x && mx <= windows[focused_window].x + windows[focused_window].width &&
        my >= windows[focused_window].y && my <= windows[focused_window].y + windows[focused_window].height) {
        win_hit = focused_window;
    } else {
        for(int i = 0; i < MAX_WINDOWS; i++) {
            if(windows[i].is_open && mx >= windows[i].x && mx <= windows[i].x + windows[i].width && my >= windows[i].y && my <= windows[i].y + windows[i].height) {
                win_hit = i; break;
            }
        }
    }

    // 4. Якщо клікнули по вікну - обробляємо
    if (win_hit != -1) {
        focused_window = win_hit; // Піднімаємо вікно наверх
        int wx = windows[win_hit].x; int wy = windows[win_hit].y; int ww = windows[win_hit].width;

        // Клік по кнопці "Х" (Закрити)
        if (j_c && my >= wy && my <= wy + DESKTOP_TITLE_H && mx >= wx + ww - 34 && mx <= wx + ww) {
            windows[win_hit].is_open = false; 
            focused_window = -1; 
            return;
        }
        // Клік по заголовку (Почати перетягування)
        if (j_c && my >= wy && my <= wy + DESKTOP_TITLE_H) {
            is_dragging = true; dragging_window = win_hit;
            drag_off_x = mx - wx; drag_off_y = my - wy; return;
        }
        // Клік всередині вікна (Передаємо управління конкретній програмі)
        if (windows[win_hit].on_click) {
            windows[win_hit].on_click(mx, my, j_r);
        }
    } else {
        // Якщо клікнули повз усі вікна (по робочому столу)
        if (j_c) focused_window = -1;
    }
}