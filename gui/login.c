#include "gui.h"
#include "vbe.h"
#include "rtc.h"
#include "timer.h"

static bool login_hover_cancel = false; static bool login_hover_login = false;
static bool login_pressed_cancel = false; static bool login_pressed_login = false;
static char username_buffer[32] = ""; static int username_len = 0;
static char password_buffer[32] = ""; static int password_len = 0;
static bool login_failed = false; static int active_field = 0;
static bool login_intro_started = false;
static uint32_t login_intro_tick = 0;

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

static int login_intro_offset(void) {
    if (!login_intro_started) {
        login_intro_started = true;
        login_intro_tick = timer_ticks;
    }
    uint32_t elapsed = timer_ticks - login_intro_tick;
    if (elapsed >= 18u) return 0;
    return (int)(18u - elapsed);
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
    int cx = (int)d_screen_w / 2;
    int cy = (int)d_screen_h / 2;
    int login_w = (int)d_screen_w - 32;
    if (login_w > 440) login_w = 440;
    if (login_w < 300) login_w = 300;
    int login_h = 360;
    int login_y = cy - login_h / 2 + login_intro_offset();
    int field_w = login_w - 100;
    if (field_w < 240) field_w = login_w - 40;
    int btn_gap = 20;
    int btn_w = (field_w - btn_gap) / 2;
    int btn_h = 38, btn_y = login_y + 298;
    int field_x = cx - field_w / 2;
    int cancel_x = field_x, login_btn_x = field_x + btn_w + btn_gap;

    login_hover_cancel = point_in_rect(mx, my, cancel_x, btn_y, btn_w, btn_h);
    login_hover_login = point_in_rect(mx, my, login_btn_x, btn_y, btn_w, btn_h);
    login_pressed_cancel = left_now && login_hover_cancel;
    login_pressed_login = left_now && login_hover_login;

    if (j_c) {
        if (login_hover_cancel) {
            username_len = 0; username_buffer[0] = '\0';
            password_len = 0; password_buffer[0] = '\0';
            login_failed = false; active_field = 0;
        } else if (login_hover_login) {
            try_login();
        } else {
            int field_w = win_w - 100;
    if (field_w < 240) field_w = win_w - 40;
    int field_h = 46, field_x = cx - field_w / 2;
            int user_y = login_y + 142, pass_y = login_y + 218;
            if (point_in_rect(mx, my, field_x, user_y, field_w, field_h)) active_field = 0;
            else if (point_in_rect(mx, my, field_x, pass_y, field_w, field_h)) active_field = 1;
        }
    }
}

void login_draw(uint32_t width, uint32_t height) {
    draw_cached_background();

    draw_filled_rect(0, 0, width, 34, 0x121826);
    draw_filled_rect(0, 33, width, 1, 0x465A78);
    if (main_font_data) {
        draw_rounded_rect(8, 5, 92, 24, 10, 0x253149);
        draw_filled_circle(21, 17, 8, 0x78AFFF);
        draw_ttf_string(18, 21, main_font_data, "B", 11.0f, 0xFFFFFF);
        draw_ttf_string(34, 22, main_font_data, "B-nix", 14.0f, 0xF7FAFF);
        const char* secure = "Local session";
        int sw = measure_ttf_text_width(main_font_data, secure, 13.0f);
        if ((int)width > sw + 132) {
            int pill_x = (int)width - sw - 32;
            draw_rounded_rect(pill_x, 5, sw + 24, 24, 10, 0x253149);
            draw_filled_circle(pill_x + 10, 17, 3, 0x78D6B0);
            draw_ttf_string(pill_x + 18, 22, main_font_data, secure, 13.0f, 0xD8E2EF);
        }
    }

    int cx = (int)width / 2, cy = (int)height / 2;
    int win_w = (int)width - 32;
    if (win_w > 440) win_w = 440;
    if (win_w < 300) win_w = 300;
    int win_h = 360;
    int wx = cx - win_w / 2;
    int wy = cy - win_h / 2 + login_intro_offset();

    draw_rounded_rect(wx + 10, wy + 14, win_w, win_h, 22, 0x0D1320);
    draw_rounded_rect(wx + 5, wy + 8, win_w, win_h, 22, 0x182238);
    draw_rounded_rect(wx, wy, win_w, win_h, 22, 0xE8EDF5);
    draw_rounded_rect(wx + 2, wy + 2, win_w - 4, win_h - 4, 20, 0xF7F9FC);

    draw_filled_circle(cx, wy + 58, 34, 0xDCE9FF);
    draw_filled_circle(cx, wy + 58, 29, 0x6EA8FF);
    if (main_font_data) {
        draw_ttf_string(cx - 10, wy + 70, main_font_data, "B", 27.0f, 0xFFFFFF);
        const char* title = "Welcome to B-nix";
        int tw = measure_ttf_text_width(main_font_data, title, 22.0f);
        draw_ttf_string(cx - tw / 2, wy + 112, main_font_data, title, 22.0f, 0x1E293B);
        const char* subtitle = "Sign in to continue to your desktop";
        int subw = measure_ttf_text_width(main_font_data, subtitle, 13.0f);
        draw_ttf_string(cx - subw / 2, wy + 132, main_font_data, subtitle, 13.0f, 0x718096);
    }

    int field_w = 340, field_h = 46, field_x = cx - field_w / 2;
    int user_y = wy + 142, pass_y = wy + 218;

    if (main_font_data) draw_ttf_string(field_x, user_y - 7, main_font_data, "Username", 12.0f, 0x526176);
    draw_rounded_rect(field_x - 2, user_y - 2, field_w + 4, field_h + 4, 11,
                      active_field == 0 ? 0x6EA8FF : 0xD4DBE5);
    draw_rounded_rect(field_x, user_y, field_w, field_h, 9, 0xFFFFFF);
    if (main_font_data) {
        const char* text = username_len > 0 ? username_buffer : "admin";
        uint32_t color = username_len > 0 ? 0x1F2937 : 0xA1AAB8;
        draw_ttf_string(field_x + 15, user_y + 30, main_font_data, text, 17.0f, color);
        if (active_field == 0 && ((timer_ticks / 25) & 1u) == 0u) {
            int px = username_len > 0 ? measure_ttf_text_width(main_font_data, username_buffer, 17.0f) : 0;
            draw_filled_rect(field_x + 15 + px, user_y + 12, 2, 22, 0x4B8EF7);
        }
    }

    if (main_font_data) draw_ttf_string(field_x, pass_y - 7, main_font_data, "Password", 12.0f, 0x526176);
    draw_rounded_rect(field_x - 2, pass_y - 2, field_w + 4, field_h + 4, 11,
                      active_field == 1 ? 0x6EA8FF : 0xD4DBE5);
    draw_rounded_rect(field_x, pass_y, field_w, field_h, 9, 0xFFFFFF);
    int dot_pitch = 13;
    if (main_font_data) {
        if (password_len == 0) {
            draw_ttf_string(field_x + 15, pass_y + 30, main_font_data, "Enter password", 17.0f, 0xA1AAB8);
        } else {
            for (int i = 0; i < password_len; i++)
                draw_filled_circle(field_x + 20 + i * dot_pitch, pass_y + 23, 4, 0x334155);
        }
        if (active_field == 1 && ((timer_ticks / 25) & 1u) == 0u)
            draw_filled_rect(field_x + 18 + password_len * dot_pitch, pass_y + 12, 2, 22, 0x4B8EF7);
    }

    if (login_failed && main_font_data)
        draw_ttf_string(field_x, pass_y + 63, main_font_data,
                        "Incorrect username or password", 13.0f, 0xD83A52);

    int btn_gap = 20;
    int btn_w = (field_w - btn_gap) / 2;
    int btn_h = 38, btn_y = wy + 298;
    int cancel_x = field_x, login_x = field_x + btn_w + btn_gap;
    uint32_t cancel_fill = login_pressed_cancel ? 0xD5DBE5 : (login_hover_cancel ? 0xE2E7EF : 0xEDF1F6);
    draw_rounded_rect(cancel_x, btn_y, btn_w, btn_h, 10, cancel_fill);
    draw_rect_outline(cancel_x, btn_y, btn_w, btn_h, 0xC8D0DC);

    uint32_t login_fill = login_pressed_login ? 0x3978D8 : (login_hover_login ? 0x77AEFF : 0x5795F2);
    draw_rounded_rect(login_x, btn_y, btn_w, btn_h, 10, login_fill);

    if (main_font_data) {
        int cw = measure_ttf_text_width(main_font_data, "Clear", 15.0f);
        draw_ttf_string(cancel_x + (btn_w - cw) / 2, btn_y + 25, main_font_data, "Clear", 15.0f, 0x334155);
        int lw = measure_ttf_text_width(main_font_data, "Continue", 15.0f);
        draw_ttf_string(login_x + (btn_w - lw) / 2, btn_y + 25, main_font_data, "Continue", 15.0f, 0xFFFFFF);
    }
}
