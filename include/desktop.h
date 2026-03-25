#ifndef DESKTOP_H
#define DESKTOP_H

#include <stdint.h>
#include <stdbool.h>

// Стани системи
typedef enum {
    STATE_LOGIN,
    STATE_DESKTOP
} os_state_t;

extern os_state_t current_state;
extern bool term_win_open;
extern uint8_t* main_font_data;

// Головні функції GUI
void desktop_init(uint32_t w, uint32_t h);
void term_gui_feed(char c);
void desktop_handle_keypress(char c);
void desktop_process_mouse(int mx, int my, bool left_now, bool right_now, bool j_c, bool j_r);
void desktop_draw(void);

#endif