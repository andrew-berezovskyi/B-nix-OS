#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include <stdbool.h>
#include "desktop.h" // Підключаємо старий файл, щоб взяти звідти os_state_t та main_font_data

// Залишаємо тільки те, чого немає в desktop.h
extern uint32_t d_screen_w;
extern uint32_t d_screen_h;
extern void wm_init(void);
// Функції малювання (render.c)
void draw_rect_outline(int x, int y, int w, int h, uint32_t color);
void draw_filled_rect(int x, int y, int w, int h, uint32_t color);
void draw_kali_window_frame(int x, int y, int ww, int wh, bool active, const char* title);
void draw_desktop_chrome(uint32_t w, uint32_t h);

// Екран входу (login.c)
void login_draw(uint32_t width, uint32_t height);
void login_process_mouse(int mx, int my, bool left_now, bool j_c);
void login_handle_keypress(char c);

// Віконний менеджер (wm.c)
void wm_draw_windows(void);
void wm_process_mouse(int mx, int my, bool left_now, bool right_now, bool j_c, bool j_r);
void wm_handle_keypress(char c);

#define MAX_WINDOWS 10

// Наш універсальний шаблон вікна
typedef struct {
    bool is_open;
    int x, y, width, height;
    char title[32];
    
    // ВКАЗІВНИКИ НА ФУНКЦІЇ (Це як методи класу в ООП)
    // Кожне вікно матиме свою унікальну функцію малювання нутрощів
    void (*draw_content)(int win_x, int win_y, int win_w, int win_h);
    
    // І свою унікальну функцію обробки кліків (наприклад, клік по папці)
    void (*on_click)(int mx, int my, bool right_click);
    
    // І свою обробку клавіатури (для терміналу чи блокнота)
    void (*on_keypress)(char c);
} window_t;

// Масив усіх вікон у системі
extern window_t windows[MAX_WINDOWS];

#endif