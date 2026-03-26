#include "gui.h"
#include "vbe.h"
#include "fs.h"
#include "shell.h"
#include "ata.h"
#include "kheap.h"

#define DESKTOP_TOP_BAR_H   32
#define DESKTOP_DOCK_W      60
#define DESKTOP_TITLE_H     32

window_t windows[MAX_WINDOWS];
int focused_window = -1; 
bool is_dragging = false;
int dragging_window = -1;
int drag_off_x = 0; int drag_off_y = 0;

extern size_t strlen(const char* str);
static void custom_strcpy(char* dst, const char* src) { while (*src) *dst++ = *src++; *dst = '\0'; }
static int custom_strcmp(const char *s1, const char *s2) { while (*s1 && (*s1 == *s2)) { s1++; s2++; } return *(const unsigned char*)s1 - *(const unsigned char*)s2; }
static int decimal_len(int n) { int len = 1; while (n >= 10) { n /= 10; len++; } return len; }
static void append_uint(char* out, int n) { int len = decimal_len(n); out[len] = '\0'; for (int i = len - 1; i >= 0; i--) { out[i] = '0' + (n % 10); n /= 10; } }

#define MAX_CACHED_FILES 64
typedef struct { char name[32]; uint8_t type; } cached_file_t;
static cached_file_t fm_file_cache[MAX_CACHED_FILES];
static int fm_file_count = -1; 

static void refresh_fm_cache(void) {
    fm_file_count = 0;
    int dir_fd = fs_opendir("/");
    if (dir_fd != -1) {
        fs_dirent_t entry;
        while (fs_readdir(dir_fd, &entry) && fm_file_count < MAX_CACHED_FILES) {
            custom_strcpy(fm_file_cache[fm_file_count].name, entry.name);
            fm_file_cache[fm_file_count].type = entry.type;
            fm_file_count++;
        }
        fs_closedir(dir_fd);
    }
}

static void vfs_create_named_file(const char* prefix, const char* ext, bool is_folder) {
    char name[32]; static int counter = 1; int p = 0;
    for (int i = 0; prefix[i] && p < 20; i++) name[p++] = prefix[i];
    char num[8]; append_uint(num, counter++);
    for (int i = 0; num[i] && p < 26; i++) name[p++] = num[i];
    for (int i = 0; ext[i] && p < 31; i++) name[p++] = ext[i];
    name[p] = '\0';
    if (is_folder) { fs_mkdir(name); } else { int fd = fs_open(name, O_CREAT | O_WRONLY); if (fd != -1) fs_close(fd); }
    fm_file_count = -1; 
}

// ==============================================================================
// ПРОГРАМИ (Координати тепер ВІДНОСНІ до вікна!)
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
    if (c == '\b') { if (term_cur_len > 0) { term_cur_len--; term_cur_line[term_cur_len] = '\0'; } windows[0].is_dirty = true; return; }
    if (c == '\n' || c == '\r') { if (c == '\n') term_gui_flush_line(); windows[0].is_dirty = true; return; }
    if (term_cur_len < TERM_LINE_W - 2) { term_cur_line[term_cur_len++] = c; term_cur_line[term_cur_len] = '\0'; windows[0].is_dirty = true; }
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

static int fm_nav_sel = 0;
static const char* fm_nav_paths[] = { "/", "/boot", "/etc", "/var" };
#define FM_NAV_COUNT 4
static char selected_filename[32] = "";
static bool context_menu_open = false; static int context_x = 0; static int context_y = 0;
static char viewer_content[4096]; 

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
    
    if (fm_file_count == -1) refresh_fm_cache();

    for (int i = 0; i < fm_file_count; i++) {
        cached_file_t* entry = &fm_file_cache[i];
        if (custom_strcmp(selected_filename, entry->name) == 0) draw_filled_rect(fx - 4, fy - 4, 84, 88, 0xD0E8FF);
        if (entry->type == FS_TYPE_DIR) draw_icon_folder(fx, fy); else draw_icon_file(fx + 4, fy - 8); 
        if (main_font_data) draw_ttf_string(fx - 4, fy + 56, main_font_data, entry->name, 11.0f, 0x222222);
        col++; if (col >= max_cols) { col = 0; fx = gx; fy += 100; } else fx += 92;
    }

    if (context_menu_open) {
        draw_filled_rect(context_x, context_y, 180, 84, 0x111111); draw_rect_outline(context_x, context_y, 180, 84, 0x4488FF);
        if (main_font_data) {
            draw_ttf_string(context_x + 8, context_y + 14, main_font_data, "New Folder", 12.0f, 0xCFE8FF);
            draw_ttf_string(context_x + 8, context_y + 38, main_font_data, "New Text File", 12.0f, 0xCFE8FF);
            draw_ttf_string(context_x + 8, context_y + 62, main_font_data, "Delete (WIP)", 12.0f, (selected_filename[0] != '\0') ? 0xFF8888 : 0x666666);
        }
    }
}

void app_fm_click(int mx, int my, bool right_click) {
    int win_idx = 1; int wx = windows[win_idx].x; int wy = windows[win_idx].y;
    int local_mx = mx - wx; int local_my = my - wy; // Працюємо локально!

    if (right_click) { context_menu_open = true; context_x = local_mx; context_y = local_my; windows[1].is_dirty = true; return; }
    if (context_menu_open) {
        if (local_mx >= context_x && local_mx <= context_x + 180 && local_my >= context_y && local_my <= context_y + 84) {
            int item = (local_my - context_y) / 28; if (item > 2) item = 2; if (item < 0) item = 0;
            if (item == 0) vfs_create_named_file("Folder", "", true); 
            else if (item == 1) vfs_create_named_file("File", ".txt", false);
        }
        context_menu_open = false; windows[1].is_dirty = true; return;
    }

    int left_w = 168; int inner_y = DESKTOP_TITLE_H;
    if (local_mx >= 0 && local_mx < left_w) {
        for (int i = 0; i < FM_NAV_COUNT; i++) {
            int ly = inner_y + 12 + i * 28;
            if (local_my >= ly - 4 && local_my < ly + 24) { fm_nav_sel = i; selected_filename[0] = '\0'; windows[1].is_dirty = true; return; }
        }
        return;
    }

    int gx = left_w + 16; int gy = inner_y + 16;
    int max_cols = (windows[win_idx].width - left_w - 32) / 92; if (max_cols < 1) max_cols = 1;
    int fx = gx, fy = gy, col = 0; bool hit_file = false;
    
    if (fm_file_count == -1) refresh_fm_cache();

    for (int i = 0; i < fm_file_count; i++) {
        cached_file_t* entry = &fm_file_cache[i];
        if (local_mx >= fx - 4 && local_mx < fx + 84 && local_my >= fy - 4 && local_my < fy + 88) {
            hit_file = true;
            if (custom_strcmp(selected_filename, entry->name) == 0 && entry->type == FS_TYPE_FILE) {
                int fd = fs_open(entry->name, O_RDONLY);
                if (fd != -1) {
                    int bytes = fs_read(fd, viewer_content, sizeof(viewer_content) - 1);
                    if (bytes >= 0) viewer_content[bytes] = '\0'; 
                    fs_close(fd);
                    windows[2].is_open = true; windows[2].is_dirty = true;
                    custom_strcpy(windows[2].title, entry->name);
                    focused_window = 2;
                }
            } else { custom_strcpy(selected_filename, entry->name); windows[1].is_dirty = true; }
            break;
        }
        col++; if (col >= max_cols) { col = 0; fx = gx; fy += 100; } else fx += 92;
    }
    if (!hit_file && selected_filename[0] != '\0') { selected_filename[0] = '\0'; windows[1].is_dirty = true; }
}

void app_viewer_draw(int x, int y, int w, int h) {
    if (main_font_data) draw_ttf_string(x + 12, y + 16, main_font_data, viewer_content, 13.0f, 0xD0D0D0);
}

// ==============================================================================
// СПРАВЖНІЙ КОМПОЗИТНИЙ WINDOW MANAGER
// ==============================================================================
void draw_kali_window_frame(int x, int y, int ww, int wh, bool active, const char* title) {
    uint32_t border = active ? 0x00A0C8 : 0x3A3A3A;
    draw_filled_rect(x, y, ww, wh, border); 
    draw_filled_rect(x + 1, y + 1, ww - 2, wh - 2, 0x1A1A1A); 
    
    uint32_t title_bg = active ? 0x252A30 : 0x1E1E1E;
    draw_filled_rect(x + 1, y + 1, ww - 2, DESKTOP_TITLE_H - 1, title_bg);
    if (active) draw_filled_rect(x + 1, y + 1, ww - 2, 3, 0x00B4D8); 
    
    if (main_font_data) draw_ttf_string(x + 10, y + 22, main_font_data, title, 14.0f, 0xE0E0E0);
    
    int cl_x = x + ww - 34; 
    draw_filled_rect(cl_x, y + 4, 28, 24, 0xC03030);
    draw_rect_outline(cl_x, y + 4, 28, 24, 0xA02020); 
    if (main_font_data) draw_ttf_string(cl_x + 9, y + 21, main_font_data, "X", 13.0f, 0xFFFFFF);
}

void wm_init(void) {
    for(int i=0; i<MAX_WINDOWS; i++) {
        windows[i].is_open = false;
        windows[i].is_dirty = true;
        windows[i].buffer = NULL;
    }

    windows[0].is_open = false; windows[0].x = 72; windows[0].y = 52; windows[0].width = 540; windows[0].height = 360;
    custom_strcpy(windows[0].title, "Terminal"); windows[0].draw_content = app_term_draw;
    windows[0].on_keypress = app_term_key; windows[0].on_click = NULL;
    windows[0].buffer = kmalloc(windows[0].width * windows[0].height * 4);

    windows[1].is_open = false; windows[1].x = 300; windows[1].y = 200; windows[1].width = 600; windows[1].height = 400;
    custom_strcpy(windows[1].title, "Files"); windows[1].draw_content = app_fm_draw;
    windows[1].on_keypress = NULL; windows[1].on_click = app_fm_click;
    windows[1].buffer = kmalloc(windows[1].width * windows[1].height * 4);
    
    windows[2].is_open = false; windows[2].x = 400; windows[2].y = 150; windows[2].width = 450; windows[2].height = 300;
    custom_strcpy(windows[2].title, "Text Viewer"); windows[2].draw_content = app_viewer_draw;
    windows[2].on_keypress = NULL; windows[2].on_click = NULL;
    windows[2].buffer = kmalloc(windows[2].width * windows[2].height * 4);

    focused_window = -1; fm_file_count = -1;
}

void wm_draw_windows(void) {
    // 1. Оновлення брудних буферів (Рендер ізольовано)
    for(int i=0; i<MAX_WINDOWS; i++) {
        if(windows[i].is_open && windows[i].is_dirty) {
            set_render_target(windows[i].buffer, windows[i].width, windows[i].height);
            bool active = (i == focused_window);
            
            draw_kali_window_frame(0, 0, windows[i].width, windows[i].height, active, windows[i].title);
            if(windows[i].draw_content) {
                windows[i].draw_content(0, DESKTOP_TITLE_H, windows[i].width, windows[i].height - DESKTOP_TITLE_H);
            }
            reset_render_target();
            windows[i].is_dirty = false;
        }
    }
    
    // 2. Композиція (Швидке накладання готових вікон на екран)
    for(int i=0; i<MAX_WINDOWS; i++) {
        if(windows[i].is_open && i != focused_window) {
            draw_buffer_to_screen(windows[i].buffer, windows[i].width, windows[i].height, windows[i].x, windows[i].y);
        }
    }
    
    // Активне вікно малюється останнім
    if (focused_window >= 0 && windows[focused_window].is_open) {
        int i = focused_window;
        draw_buffer_to_screen(windows[i].buffer, windows[i].width, windows[i].height, windows[i].x, windows[i].y);
    }
}

void wm_handle_keypress(char c) {
    if (focused_window >= 0 && windows[focused_window].is_open && windows[focused_window].on_keypress) {
        windows[focused_window].on_keypress(c);
        windows[focused_window].is_dirty = true;
    }
}

void wm_process_mouse(int mx, int my, bool left_now, bool right_now, bool j_c, bool j_r) {
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

    if (!j_c && !j_r) return;

    if (mx < DESKTOP_DOCK_W && my >= DESKTOP_TOP_BAR_H) {
        int di = (my - DESKTOP_TOP_BAR_H) / 58;
        if (di == 0) { windows[0].is_open = true; focused_window = 0; windows[0].is_dirty = true; }
        if (di == 1) { windows[1].is_open = true; focused_window = 1; windows[1].is_dirty = true; }
        return;
    }

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

    int old_focused = focused_window;

    if (win_hit != -1) {
        focused_window = win_hit;
        if (focused_window != old_focused) {
            if (old_focused != -1) windows[old_focused].is_dirty = true;
            windows[focused_window].is_dirty = true;
        }

        int wx = windows[win_hit].x; int wy = windows[win_hit].y; int ww = windows[win_hit].width;

        if (j_c && my >= wy && my <= wy + DESKTOP_TITLE_H && mx >= wx + ww - 34 && mx <= wx + ww) {
            windows[win_hit].is_open = false; focused_window = -1; windows[win_hit].is_dirty = true; return;
        }
        if (j_c && my >= wy && my <= wy + DESKTOP_TITLE_H) {
            is_dragging = true; dragging_window = win_hit;
            drag_off_x = mx - wx; drag_off_y = my - wy; return;
        }
        if (windows[win_hit].on_click) windows[win_hit].on_click(mx, my, j_r);
    } else {
        if (j_c) {
            focused_window = -1;
            if (old_focused != -1) windows[old_focused].is_dirty = true;
        }
    }
}