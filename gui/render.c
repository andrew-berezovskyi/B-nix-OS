#include "gui.h"
#include "vbe.h"
#include "rtc.h"

#define DESKTOP_TOP_BAR_H   34
#define DESKTOP_DOCK_W      360
#define DESKTOP_DOCK_H      70
#define DESKTOP_DOCK_MARGIN 18

// УВАГА: draw_rect_outline та draw_filled_rect перенесено у vbe.c для апаратного прискорення!

void draw_filled_circle(int x, int y, int r, uint32_t color) {
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy <= r*r) draw_pixel(x + dx, y + dy, color);
        }
    }
}

void draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    draw_filled_rect(x + r, y, w - 2*r, h, color);
    draw_filled_rect(x, y + r, w, h - 2*r, color);
    draw_filled_circle(x + r, y + r, r, color);
    draw_filled_circle(x + w - r, y + r, r, color);
    draw_filled_circle(x + r, y + h - r, r, color);
    draw_filled_circle(x + w - r, y + h - r, r, color);
}

static int rtc_weekday_sun0(uint32_t year, uint8_t month, uint8_t day) {
    const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) year--;
    return (int)((year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7);
}

static void format_rtc_datetime_kali(char* out, size_t cap) {
    uint8_t hour, min, sec, d, mo; uint32_t y; read_rtc(&hour, &min, &sec, &d, &mo, &y);
    if (cap < 28) { out[0] = '\0'; return; }
    static const char* wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* mn[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int wdi = rtc_weekday_sun0(y, mo, d); if (wdi < 0) wdi = 0; if (wdi > 6) wdi = 6;
    const char* wds = wd[wdi]; const char* ms = mn[(mo >= 1 && mo <= 12) ? (mo - 1) : 0];
    size_t p = 0; while (wds[0] && p + 1 < cap) out[p++] = *wds++;
    if (p + 1 < cap) out[p++] = ' '; while (ms[0] && p + 1 < cap) out[p++] = *ms++;
    if (p + 1 < cap) out[p++] = ' ';
    if (d >= 10) { if (p + 1 < cap) out[p++] = (char)('0' + (d / 10)); if (p + 1 < cap) out[p++] = (char)('0' + (d % 10)); } else { if (p + 1 < cap) out[p++] = (char)('0' + d); }
    if (p + 1 < cap) out[p++] = ','; if (p + 1 < cap) out[p++] = ' ';
    if (p + 1 < cap) out[p++] = (char)('0' + (hour / 10)); if (p + 1 < cap) out[p++] = (char)('0' + (hour % 10));
    if (p + 1 < cap) out[p++] = ':';
    if (p + 1 < cap) out[p++] = (char)('0' + (min / 10)); if (p + 1 < cap) out[p++] = (char)('0' + (min % 10));
    out[p] = '\0';
}

static void draw_desktop_dock(uint32_t w, uint32_t h) {
    int dock_x = ((int)w - DESKTOP_DOCK_W) / 2;
    int dock_y = (int)h - DESKTOP_DOCK_H - DESKTOP_DOCK_MARGIN;
    draw_rounded_rect(dock_x + 3, dock_y + 5, DESKTOP_DOCK_W, DESKTOP_DOCK_H, 18, 0x111827);
    draw_rounded_rect(dock_x, dock_y, DESKTOP_DOCK_W, DESKTOP_DOCK_H, 18, 0xE8EDF5);
    draw_rounded_rect(dock_x + 2, dock_y + 2, DESKTOP_DOCK_W - 4, DESKTOP_DOCK_H - 4, 16, 0xC8D1DF);
    draw_rounded_rect(dock_x + 3, dock_y + 3, DESKTOP_DOCK_W - 6, DESKTOP_DOCK_H - 6, 15, 0x202938);
}

static void draw_dock_icon(int x, int y, uint32_t accent, const char* label, bool running) {
    draw_rounded_rect(x + 2, y + 3, 48, 48, 12, 0x111722);
    draw_rounded_rect(x, y, 48, 48, 12, accent);
    draw_rounded_rect(x + 5, y + 5, 38, 38, 9, 0xF7FAFF);
    if (main_font_data) {
        int tw = measure_ttf_text_width(main_font_data, label, 17.0f);
        draw_ttf_string(x + (48 - tw) / 2, y + 31, main_font_data, label, 17.0f, 0x172033);
    }
    if (running) draw_rounded_rect(x + 18, y + 55, 12, 3, 1, 0x70A7FF);
}

static void draw_status_tray_icons(int x, int y0) {
    /* Wi-Fi-like signal, intentionally original B-nix geometry. */
    draw_filled_circle(x + 8, y0 + 9, 2, 0xE8EEF8);
    draw_rect_outline(x + 4, y0 + 5, 9, 7, 0xAFC0D8);
    draw_rect_outline(x + 1, y0 + 2, 15, 12, 0x70839D);

    x += 25;
    draw_rounded_rect(x, y0 + 2, 21, 11, 3, 0xAFC0D8);
    draw_rounded_rect(x + 2, y0 + 4, 15, 7, 2, 0x78D6B0);
    draw_filled_rect(x + 21, y0 + 5, 2, 5, 0xAFC0D8);
}

void draw_top_bar_kali(uint32_t w) {
    /* A compact floating control surface instead of the old solid taskbar. */
    draw_filled_rect(0, 0, w, DESKTOP_TOP_BAR_H, 0x121826);
    draw_filled_rect(0, DESKTOP_TOP_BAR_H - 1, w, 1, 0x465A78);

    if (!main_font_data) return;

    draw_rounded_rect(8, 5, 92, 24, 10, 0x253149);
    draw_filled_circle(21, 17, 8, 0x78AFFF);
    draw_ttf_string(18, 21, main_font_data, "B", 11.0f, 0xFFFFFF);
    draw_ttf_string(34, 22, main_font_data, "B-nix", 14.0f, 0xF7FAFF);

    if (w >= 700) {
        draw_rounded_rect(108, 5, 92, 24, 10, 0x1B2537);
        draw_ttf_string(123, 22, main_font_data, "Workspace", 13.0f, 0xB9C6D8);
    }

    char dt[32];
    format_rtc_datetime_kali(dt, sizeof(dt));
    int dtw = measure_ttf_text_width(main_font_data, dt, 13.0f);
    int tray_w = dtw + 72;
    if (tray_w < 178) tray_w = 178;
    int tray_x = (int)w - tray_w - 8;
    if (tray_x < 210) tray_x = 210;
    draw_rounded_rect(tray_x, 5, tray_w, 24, 10, 0x253149);
    draw_status_tray_icons(tray_x + 12, 9);
    draw_ttf_string(tray_x + 58, 22, main_font_data, dt, 13.0f, 0xEDF3FA);

    if (w >= 980) {
        const char* state = "Aurora desktop";
        int sw = measure_ttf_text_width(main_font_data, state, 13.0f);
        draw_ttf_string(((int)w - sw) / 2, 22, main_font_data, state, 13.0f, 0xAFC0D8);
    }
}

void draw_desktop_chrome(uint32_t w, uint32_t h) {
    draw_top_bar_kali(w);
    draw_desktop_dock(w, h);

    int dock_x = ((int)w - DESKTOP_DOCK_W) / 2;
    int dock_y = (int)h - DESKTOP_DOCK_H - DESKTOP_DOCK_MARGIN;
    draw_dock_icon(dock_x + 18,  dock_y + 10, 0x6EA8FF, ">", windows[0].is_open);
    draw_dock_icon(dock_x + 78,  dock_y + 10, 0x78D6B0, "F", windows[1].is_open);
    draw_dock_icon(dock_x + 138, dock_y + 10, 0xF2C66D, "N", windows[2].is_open);
    draw_filled_rect(dock_x + 202, dock_y + 13, 1, 43, 0x526176);
    draw_dock_icon(dock_x + 218, dock_y + 10, 0xB39DDB, "A", windows[3].is_open);
    draw_dock_icon(dock_x + 278, dock_y + 10, 0xF08C8C, "S", windows[4].is_open);
}
