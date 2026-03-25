#include "desktop.h"
#include "gui.h"
#include "vbe.h"

os_state_t current_state = STATE_LOGIN;

uint32_t d_screen_w = 0;
uint32_t d_screen_h = 0;

void desktop_init(uint32_t w, uint32_t h) {
    d_screen_w = w;
    d_screen_h = h;
    wm_init();
}

void desktop_handle_keypress(char c) {
    if (current_state == STATE_LOGIN) {
        login_handle_keypress(c);
    } else {
        wm_handle_keypress(c);
    }
}

void desktop_process_mouse(int mx, int my, bool left_now, bool right_now, bool j_c, bool j_r) {
    if (current_state == STATE_LOGIN) {
        login_process_mouse(mx, my, left_now, j_c);
    } else {
        wm_process_mouse(mx, my, left_now, right_now, j_c, j_r);
    }
}

void desktop_draw(void) {
    if (current_state == STATE_LOGIN) {
        login_draw(d_screen_w, d_screen_h);
    } else {
        draw_cached_background();
        draw_desktop_chrome(d_screen_w, d_screen_h);
        wm_draw_windows();
    }
}