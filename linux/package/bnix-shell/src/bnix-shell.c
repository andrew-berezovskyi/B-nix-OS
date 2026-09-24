#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

struct surface {
    uint8_t *pixels;
    size_t length;
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
};

enum page { PAGE_HOME, PAGE_FILES, PAGE_NETWORK, PAGE_SETTINGS, PAGE_ABOUT };

static const uint8_t font[128][5] = {
    [' ']={0,0,0,0,0}, ['-']={8,8,8,8,8}, ['.']={0,0,0,16,0},
    ['/']={16,8,4,2,1}, [':']={0,20,0,20,0},
    ['0']={31,17,17,17,31}, ['1']={0,18,31,16,0},
    ['2']={29,21,21,21,23}, ['3']={17,21,21,21,31},
    ['4']={7,4,4,4,31}, ['5']={23,21,21,21,29},
    ['6']={31,21,21,21,29}, ['7']={1,1,25,5,3},
    ['8']={31,21,21,21,31}, ['9']={23,21,21,21,31},
    ['A']={30,5,5,5,30}, ['B']={31,21,21,21,10},
    ['C']={14,17,17,17,10}, ['D']={31,17,17,17,14},
    ['E']={31,21,21,21,17}, ['F']={31,5,5,5,1},
    ['G']={14,17,21,21,29}, ['H']={31,4,4,4,31},
    ['I']={17,17,31,17,17}, ['J']={8,16,16,16,15},
    ['K']={31,4,10,17,0}, ['L']={31,16,16,16,16},
    ['M']={31,2,4,2,31}, ['N']={31,2,4,8,31},
    ['O']={14,17,17,17,14}, ['P']={31,5,5,5,2},
    ['Q']={14,17,25,17,30}, ['R']={31,5,13,21,18},
    ['S']={18,21,21,21,9}, ['T']={1,1,31,1,1},
    ['U']={15,16,16,16,15}, ['V']={7,8,16,8,7},
    ['W']={31,8,4,8,31}, ['X']={17,10,4,10,17},
    ['Y']={1,2,28,2,1}, ['Z']={25,21,21,19,0}
};

static uint32_t channel(unsigned value, const struct fb_bitfield *field) {
    if (field->length == 0) return 0;
    unsigned max = (1u << field->length) - 1u;
    return (value * max / 255u) << field->offset;
}

static uint32_t color(const struct surface *s, unsigned r, unsigned g, unsigned b) {
    return channel(r, &s->var.red) | channel(g, &s->var.green) |
           channel(b, &s->var.blue);
}

static void pixel(struct surface *s, int x, int y, uint32_t value) {
    if (x < 0 || y < 0 || x >= (int)s->var.xres || y >= (int)s->var.yres) return;
    size_t offset = (size_t)(y + (int)s->var.yoffset) * s->fix.line_length +
                    (size_t)(x + (int)s->var.xoffset) * (s->var.bits_per_pixel / 8u);
    if (offset + s->var.bits_per_pixel / 8u > s->length) return;
    if (s->var.bits_per_pixel == 32) *(uint32_t *)(s->pixels + offset) = value;
    else *(uint16_t *)(s->pixels + offset) = (uint16_t)value;
}

static void rect(struct surface *s, int x, int y, int w, int h, int radius, uint32_t c) {
    if (w <= 0 || h <= 0) return;
    if (radius < 0) radius = 0;
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;
    for (int py = 0; py < h; ++py) {
        for (int px = 0; px < w; ++px) {
            int dx = px < radius ? radius - px : (px >= w - radius ? px - (w - radius - 1) : 0);
            int dy = py < radius ? radius - py : (py >= h - radius ? py - (h - radius - 1) : 0);
            if (dx * dx + dy * dy <= radius * radius) pixel(s, x + px, y + py, c);
        }
    }
}

static void circle(struct surface *s, int cx, int cy, int r, uint32_t c) {
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x)
            if (x * x + y * y <= r * r) pixel(s, cx + x, cy + y, c);
}

static void text(struct surface *s, int x, int y, const char *value, int scale, uint32_t c) {
    for (; *value; ++value) {
        unsigned char ch = (unsigned char)*value;
        if (ch >= 'a' && ch <= 'z') ch = (unsigned char)(ch - 'a' + 'A');
        if (ch >= 128) ch = ' ';
        for (int col = 0; col < 5; ++col)
            for (int row = 0; row < 7; ++row)
                if ((font[ch][col] >> row) & 1u)
                    rect(s, x + col * scale, y + row * scale, scale, scale, 0, c);
        x += 6 * scale;
    }
}

static void draw_files(struct surface *s, int x, int y, uint32_t fg, uint32_t muted) {
    text(s, x, y, "FILES  ROOT DIRECTORY", 2, fg);
    DIR *dir = opendir("/");
    if (!dir) {
        text(s, x, y + 34, "UNABLE TO READ ROOT", 2, muted);
        return;
    }
    struct dirent *entry;
    int row = 0;
    while ((entry = readdir(dir)) != NULL && row < 7) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char line[48];
        (void)snprintf(line, sizeof line, "/%s", entry->d_name);
        text(s, x, y + 36 + row * 25, line, 2, row == 0 ? fg : muted);
        ++row;
    }
    closedir(dir);
}

static void draw_network(struct surface *s, int x, int y, uint32_t fg, uint32_t muted) {
    text(s, x, y, "NETWORK  LIVE STATUS", 2, fg);
    struct ifaddrs *interfaces = NULL;
    char address[INET_ADDRSTRLEN] = "NOT CONNECTED";
    if (getifaddrs(&interfaces) == 0) {
        for (struct ifaddrs *it = interfaces; it; it = it->ifa_next) {
            if (it->ifa_addr && it->ifa_addr->sa_family == AF_INET &&
                !strcmp(it->ifa_name, "eth0")) {
                struct sockaddr_in *in = (struct sockaddr_in *)it->ifa_addr;
                if (!inet_ntop(AF_INET, &in->sin_addr, address, sizeof address))
                    (void)snprintf(address, sizeof address, "ADDRESS ERROR");
                break;
            }
        }
        freeifaddrs(interfaces);
    }
    char line[64];
    (void)snprintf(line, sizeof line, "ETH0  %s", address);
    text(s, x, y + 44, line, 2, fg);
    text(s, x, y + 82, "VIRTIO NETWORK ADAPTER", 2, muted);
    text(s, x, y + 116, "DHCP AND HTTPS ENABLED", 2, muted);
}

static void draw_settings(struct surface *s, int x, int y, uint32_t fg, uint32_t muted) {
    text(s, x, y, "SYSTEM SETTINGS", 2, fg);
    char hostname[64] = "B-NIX";
    (void)gethostname(hostname, sizeof hostname);
    hostname[sizeof hostname - 1] = '\0';
    char line[80];
    (void)snprintf(line, sizeof line, "DEVICE  %s", hostname);
    text(s, x, y + 42, line, 2, fg);
    (void)snprintf(line, sizeof line, "DISPLAY  %u X %u", s->var.xres, s->var.yres);
    text(s, x, y + 78, line, 2, muted);
    text(s, x, y + 114, "THEME  AURORA DARK", 2, muted);
    text(s, x, y + 150, "SCALE  RESPONSIVE", 2, muted);
}

static void draw_about(struct surface *s, int x, int y, uint32_t fg, uint32_t muted) {
    text(s, x, y, "ABOUT B-NIX", 2, fg);
    struct utsname info;
    char line[96] = "LINUX";
    if (uname(&info) == 0) (void)snprintf(line, sizeof line, "LINUX  %s", info.release);
    text(s, x, y + 42, line, 2, fg);
    text(s, x, y + 82, "X86 64  MUSL  BUILDROOT", 2, muted);
    text(s, x, y + 118, "AURORA SHELL  USERSPACE", 2, muted);
    text(s, x, y + 154, "B-NIX HYBRID PLATFORM", 2, muted);
}

static void render(struct surface *s, enum page active) {
    int w = (int)s->var.xres, h = (int)s->var.yres;
    uint32_t navy = color(s, 7, 15, 34), blue = color(s, 24, 74, 146);
    uint32_t panel = color(s, 20, 31, 54), panel2 = color(s, 31, 45, 72);
    uint32_t white = color(s, 239, 246, 255), muted = color(s, 139, 160, 190);
    uint32_t accent = color(s, 86, 156, 255), mint = color(s, 91, 218, 183);
    uint32_t coral = color(s, 255, 115, 125), gold = color(s, 255, 195, 92);

    for (int y = 0; y < h; ++y) {
        unsigned mix = (unsigned)(y * 100 / (h ? h : 1));
        rect(s, 0, y, w, 1, 0, color(s, 7 + mix / 12, 15 + mix / 9, 34 + mix / 5));
    }
    circle(s, w * 4 / 5, h / 3, h / 3, blue);
    circle(s, w / 8, h * 4 / 5, h / 4, navy);

    int bar_h = h < 600 ? 42 : 50;
    rect(s, 0, 0, w, bar_h, 0, panel);
    circle(s, 27, bar_h / 2, 11, accent);
    text(s, 48, bar_h / 2 - 7, "B-NIX", 2, white);
    text(s, w / 2 - 42, bar_h / 2 - 7, "AURORA", 2, muted);

    time_t now = time(NULL);
    struct tm tm_now;
    char clock_text[16] = "00:00";
    if (localtime_r(&now, &tm_now))
        (void)snprintf(clock_text, sizeof clock_text, "%02d:%02d", tm_now.tm_hour, tm_now.tm_min);
    text(s, w - 100, bar_h / 2 - 7, clock_text, 2, white);

    int card_w = w < 900 ? w * 4 / 5 : 660;
    int card_h = h < 600 ? 220 : 280;
    int card_x = (w - card_w) / 2, card_y = (h - card_h) / 2 - 20;
    rect(s, card_x + 8, card_y + 10, card_w, card_h, 26, navy);
    rect(s, card_x, card_y, card_w, card_h, 26, panel2);
    int content_x = card_x + 42, content_y = card_y + 34;
    if (active == PAGE_FILES) draw_files(s, content_x, content_y, white, muted);
    else if (active == PAGE_NETWORK) draw_network(s, content_x, content_y, white, muted);
    else if (active == PAGE_SETTINGS) draw_settings(s, content_x, content_y, white, muted);
    else if (active == PAGE_ABOUT) draw_about(s, content_x, content_y, white, muted);
    else {
        text(s, content_x, content_y, "WELCOME TO B-NIX", w < 800 ? 2 : 3, white);
        text(s, content_x, content_y + 54, "LINUX FOUNDATION READY", 2, mint);
        text(s, content_x, content_y + 92, "REAL SYSTEM VIEWS ENABLED", 2, muted);
        text(s, content_x, content_y + 132, "PRESS 1 2 3 OR 4", 2, accent);
    }

    int dock_w = w < 700 ? w - 40 : 430, dock_h = h < 500 ? 60 : 76;
    int dock_x = (w - dock_w) / 2, dock_y = h - dock_h - 22;
    rect(s, dock_x + 5, dock_y + 7, dock_w, dock_h, 24, navy);
    rect(s, dock_x, dock_y, dock_w, dock_h, 24, panel);
    uint32_t icons[4] = {accent, mint, gold, coral};
    int gap = dock_w / 5, radius = dock_h / 3;
    for (int i = 0; i < 4; ++i) {
        circle(s, dock_x + gap * (i + 1), dock_y + dock_h / 2, radius, icons[i]);
        char label[2] = {(char)('1' + i), '\0'};
        text(s, dock_x + gap * (i + 1) - 5, dock_y + dock_h / 2 - 7, label, 2, navy);
    }
    text(s, 16, h - 15, "1 FILES  2 NETWORK  3 SETTINGS  4 ABOUT  ESC HOME", 1, muted);
}

#define BITS_PER_LONG (sizeof(unsigned long) * 8u)
#define BIT_WORD(bit) ((bit) / BITS_PER_LONG)
#define BIT_MASK(bit) (1ul << ((bit) % BITS_PER_LONG))

static int find_keyboard(void) {
    for (int index = 0; index < 16; ++index) {
        char path[32];
        (void)snprintf(path, sizeof path, "/dev/input/event%d", index);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        unsigned long keys[(KEY_MAX + BITS_PER_LONG) / BITS_PER_LONG];
        memset(keys, 0, sizeof keys);
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keys), keys) >= 0 &&
            (keys[BIT_WORD(KEY_1)] & BIT_MASK(KEY_1))) return fd;
        close(fd);
    }
    return -1;
}

static enum page key_page(unsigned code, enum page current) {
    if (code == KEY_1 || code == KEY_F) return PAGE_FILES;
    if (code == KEY_2 || code == KEY_N) return PAGE_NETWORK;
    if (code == KEY_3 || code == KEY_S) return PAGE_SETTINGS;
    if (code == KEY_4 || code == KEY_A) return PAGE_ABOUT;
    if (code == KEY_ESC || code == KEY_H) return PAGE_HOME;
    return current;
}

int main(void) {
    int fb = open("/dev/fb0", O_RDWR);
    if (fb < 0) { fprintf(stderr, "bnix-shell: open /dev/fb0: %s\n", strerror(errno)); return 1; }

    struct surface s;
    memset(&s, 0, sizeof s);
    if (ioctl(fb, FBIOGET_FSCREENINFO, &s.fix) < 0 ||
        ioctl(fb, FBIOGET_VSCREENINFO, &s.var) < 0) {
        fprintf(stderr, "bnix-shell: framebuffer query failed: %s\n", strerror(errno));
        close(fb);
        return 1;
    }
    if (s.var.bits_per_pixel != 32 && s.var.bits_per_pixel != 16) {
        fprintf(stderr, "bnix-shell: unsupported depth %u\n", s.var.bits_per_pixel);
        close(fb);
        return 1;
    }

    s.length = s.fix.smem_len ? s.fix.smem_len : (size_t)s.fix.line_length * s.var.yres;
    s.pixels = mmap(NULL, s.length, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    if (s.pixels == MAP_FAILED) {
        fprintf(stderr, "bnix-shell: mmap failed: %s\n", strerror(errno));
        close(fb);
        return 1;
    }

    int keyboard = find_keyboard();
    enum page active = PAGE_HOME;
    render(&s, active);

    int console = open("/dev/console", O_WRONLY);
    if (console >= 0) {
        dprintf(console, "[BNIX-GUI] shell ready %ux%u@%u\n", s.var.xres, s.var.yres, s.var.bits_per_pixel);
        dprintf(console, keyboard >= 0 ? "[BNIX-GUI] input ready\n" : "[BNIX-GUI] input unavailable\n");
        close(console);
    }

    for (;;) {
        if (keyboard < 0) {
            sleep(1);
            keyboard = find_keyboard();
            render(&s, active);
            continue;
        }
        struct pollfd descriptor = {.fd = keyboard, .events = POLLIN};
        int result = poll(&descriptor, 1, 1000);
        if (result > 0 && (descriptor.revents & POLLIN)) {
            struct input_event events[16];
            ssize_t count = read(keyboard, events, sizeof events);
            if (count > 0) {
                size_t total = (size_t)count / sizeof events[0];
                for (size_t i = 0; i < total; ++i)
                    if (events[i].type == EV_KEY && events[i].value == 1)
                        active = key_page(events[i].code, active);
            }
        } else if (result < 0 && errno != EINTR) {
            close(keyboard);
            keyboard = -1;
        }
        render(&s, active);
    }
}
