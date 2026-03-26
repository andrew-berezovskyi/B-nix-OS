#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include <stdbool.h>
#include "desktop.h" 

extern uint32_t d_screen_w;
extern uint32_t d_screen_h;
extern void wm_init(void);

void draw_rect_outline(int x, int y, int w, int h, uint32_t color);
void draw_filled_rect(int x, int y, int w, int h, uint32_t color);
void draw_kali_window_frame(int x, int y, int ww, int wh, bool active, const char* title);
void draw_desktop_chrome(uint32_t w, uint32_t h);
void draw_icon_folder(int x, int y);
void draw_icon_file(int x, int y);
void draw_filled_circle(int x, int y, int r, uint32_t color);
void draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);

void login_draw(uint32_t width, uint32_t height);
void login_process_mouse(int mx, int my, bool left_now, bool j_c);
void login_handle_keypress(char c);

void wm_draw_windows(void);
void wm_process_mouse(int mx, int my, bool left_now, bool right_now, bool j_c, bool j_r);
void wm_handle_keypress(char c);

#define MAX_WINDOWS 10

typedef struct {
    bool is_open;
    int x, y, width, height;
    char title[32];
    
    // 🔥 НОВЕ: Справжній Композитор
    uint32_t* buffer;  // Приватний холст вікна
    bool is_dirty;     // Чи потрібно перемалювати нутрощі вікна
    
    void (*draw_content)(int win_x, int win_y, int win_w, int win_h);
    void (*on_click)(int mx, int my, bool right_click);
    void (*on_keypress)(char c);
} window_t;

extern window_t windows[MAX_WINDOWS];

#endif