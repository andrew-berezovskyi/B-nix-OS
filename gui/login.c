#include "gui.h"
#include "vbe.h"
#include "rtc.h"
#include "timer.h"

static bool login_hover_cancel = false; static bool login_hover_login = false;
static bool login_pressed_cancel = false; static bool login_pressed_login = false;
static char username_buffer[32] = ""; static int username_len = 0;
static char password_buffer[32] = ""; static int password_len = 0;
static bool login_failed = false; static int active_field = 0;

typedef struct {
    bool authenticated; char username[32]; uint32_t password_shadow_hash;
} user_session_t;
static user_session_t user_session = { false, "", 0 };

static int custom_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void custom_strcpy(char* dst, const char* src) {
    while (*src) *dst++ = *src++; *dst = '\0';
}

static uint32_t shadow_hash_password(const char* pass) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; pass[i] != '\0'; i++) { h ^= (uint8_t)pass[i]; h *= 16777619u; }
    return h;
}

static bool point_in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

static void try_login(void) {
    uint32_t in_hash = shadow_hash_password(password_buffer);
    if (custom_strcmp(username_buffer, "admin") == 0 && in_hash == shadow_hash_password("1234")) {
        user_session.authenticated = true; custom_strcpy(user_session.username, username_buffer);
        user_session.password_shadow_hash = in_hash;
        password_buffer[0] = '\0'; password_len = 0;
        current_state = STATE_DESKTOP; login_failed = false;
    } else {
        user_session.authenticated = false; user_session.username[0] = '\0'; user_session.password_shadow_hash = 0;
        login_failed = true; password_len = 0; password_buffer[0] = '\0';
        username_len = 0; username_buffer[0] = '\0'; active_field = 0;
    }
}

void login_handle_keypress(char c) {
    if (c == '\b') {
        if (active_field == 0) { if (username_len > 0) username_buffer[--username_len] = '\0'; }
        else { if (password_len > 0) password_buffer[--password_len] = '\0'; }
    }
    else if (c == '\t') { active_field = (active_field == 0) ? 1 : 0; }
    else if (c == '\n') { try_login(); }
    else {
        if (active_field == 0) { if (username_len < 30) username_buffer[username_len++] = c; username_buffer[username_len] = '\0'; }
        else { if (password_len < 30) password_buffer[password_len++] = c; password_buffer[password_len] = '\0'; }
    }
}

void login_process_mouse(int mx, int my, bool left_now, bool j_c) {
    int cx = (int)d_screen_w / 2; int cy = (int)d_screen_h / 2;
    int login_w = 400, login_h = 218; int login_y = cy - login_h / 2;
    int btn_w = 150, btn_h = 34, btn_y = login_y + 166; int cancel_x = cx - 160, login_btn_x = cx + 10;

    login_hover_cancel = point_in_rect(mx, my, cancel_x, btn_y, btn_w, btn_h);
    login_hover_login = point_in_rect(mx, my, login_btn_x, btn_y, btn_w, btn_h);
    login_pressed_cancel = left_now && login_hover_cancel;
    login_pressed_login = left_now && login_hover_login;

    if (j_c) {
        if (login_hover_cancel) {
            username_len = 0; username_buffer[0] = '\0'; password_len = 0; password_buffer[0] = '\0';
            login_failed = false; active_field = 0;
        } else if (login_hover_login) { try_login(); } 
        else {
            int field_w = 320, field_h = 44, field_x = cx - field_w / 2;
            int user_y = login_y + 34; int pass_y = login_y + 86;
            if (point_in_rect(mx, my, field_x, user_y, field_w, field_h)) active_field = 0;
            else if (point_in_rect(mx, my, field_x, pass_y, field_w, field_h)) active_field = 1;
        }
    }
}

void login_draw(uint32_t width, uint32_t height) {
    draw_cached_background();
    const int top_h = 38; const float top_font = 22.0f; const int top_baseline_y = 28;
    draw_filled_rect(0, 0, width, top_h, 0x1A1A1A);
    if (main_font_data) {
        draw_ttf_string(12, top_baseline_y, main_font_data, "B-nix", top_font, 0xBFD3E6);
    }
    int cx = width / 2; int cy = height / 2;
    int win_w = 400; int win_h = 218; int wx = cx - win_w / 2; int wy = cy - win_h / 2;
    draw_rounded_rect(wx, wy, win_w, win_h, 16, 0x111111);
    draw_cached_icon_centered(cx, wy + 40);

    int field_w = 320; int field_h = 44; int field_x = cx - field_w / 2;
    int text_baseline_offset = 30;
    int username_text_px = measure_ttf_text_width(main_font_data, username_buffer, 18.0f);
    int dot_pitch = measure_ttf_text_width(main_font_data, "*", 18.0f); if (dot_pitch < 10) dot_pitch = 10;

    int user_y = wy + 34;
    if (active_field == 0) draw_rounded_rect(field_x - 2, user_y - 2, field_w + 4, field_h + 4, 9, 0x1C315A);
    draw_rounded_rect(field_x, user_y, field_w, field_h, 8, 0x0A0A0A);
    if (active_field == 0) { draw_rounded_rect(field_x - 1, user_y - 1, field_w + 2, field_h + 2, 8, 0x2A4F90); draw_rounded_rect(field_x, user_y, field_w, field_h, 8, 0x0A0A0A); }
    if (main_font_data) {
        if (username_len > 0) draw_ttf_string(field_x + 14, user_y + text_baseline_offset, main_font_data, username_buffer, 18.0, 0xFFFFFF);
        else draw_ttf_string(field_x + 14, user_y + text_baseline_offset, main_font_data, "Username", 18.0, 0x666D78);
    }
    if (active_field == 0 && ((timer_ticks / 25) & 1u) == 0u) {
        int caret_x = field_x + 14 + username_text_px + 1; draw_filled_rect(caret_x, user_y + 12, 2, 20, 0x88B8FF);
    }

    int pass_y = wy + 86;
    if (active_field == 1) draw_rounded_rect(field_x - 2, pass_y - 2, field_w + 4, field_h + 4, 9, 0x1C315A);
    draw_rounded_rect(field_x, pass_y, field_w, field_h, 8, 0x0A0A0A);
    if (active_field == 1) { draw_rounded_rect(field_x - 1, pass_y - 1, field_w + 2, field_h + 2, 8, 0x2A4F90); draw_rounded_rect(field_x, pass_y, field_w, field_h, 8, 0x0A0A0A); }
    if (main_font_data) {
        if (password_len > 0) { for (int i = 0; i < password_len; i++) draw_filled_circle(field_x + 20 + (i * dot_pitch), pass_y + 22, 4, 0xFFFFFF); } 
        else draw_ttf_string(field_x + 14, pass_y + text_baseline_offset, main_font_data, "Password", 18.0, 0x666D78);
    }
    if (active_field == 1 && ((timer_ticks / 25) & 1u) == 0u) {
        int caret_x = field_x + 20 + (password_len * dot_pitch) + 1; draw_filled_rect(caret_x, pass_y + 12, 2, 20, 0x88B8FF);
    }

    if (login_failed && main_font_data) draw_ttf_string(cx - 100, pass_y + 62, main_font_data, "Authentication failed!", 16.0, 0xFF3333);

    int btn_w = 150; int btn_h = 34; int btn_y = wy + 166; int cancel_x = cx - 160;
    uint32_t cancel_fill = login_pressed_cancel ? 0x1E1E1E : (login_hover_cancel ? 0x2E2E2E : 0x2A2A2A);
    uint32_t cancel_border = login_pressed_cancel ? 0x222222 : (login_hover_cancel ? 0x4A4A4A : 0x333333);
    draw_rounded_rect(cancel_x, btn_y, btn_w, btn_h, 6, cancel_fill); draw_rect_outline(cancel_x, btn_y, btn_w, btn_h, cancel_border);
    if (main_font_data) { int tw = measure_ttf_text_width(main_font_data, "Cancel", 16.0f); draw_ttf_string(cancel_x + (btn_w - tw) / 2, btn_y + 23, main_font_data, "Cancel", 16.0, 0xD4D8DE); }

    int login_x = cx + 10;
    uint32_t login_fill = login_pressed_login ? 0x2E57AD : (login_hover_login ? 0x3C72DC : 0x3366CC);
    uint32_t login_border = login_pressed_login ? 0x3A6FD8 : (login_hover_login ? 0x66A4FF : 0x4488FF);
    draw_rounded_rect(login_x, btn_y, btn_w, btn_h, 6, login_fill); draw_rect_outline(login_x, btn_y, btn_w, btn_h, login_border);
    if (main_font_data) { int tw = measure_ttf_text_width(main_font_data, "Log In", 16.0f); draw_ttf_string(login_x + (btn_w - tw) / 2, btn_y + 23, main_font_data, "Log In", 16.0, 0xFFFFFF); }
}