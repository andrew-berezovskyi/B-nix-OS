#ifndef MOUSE_H
#define MOUSE_H

#include <stdbool.h>
#include <stdint.h>

// Глобальні змінні, які ми експортуємо для ядра
extern int mouse_x;
extern int mouse_y;
extern bool mouse_left_pressed;
extern bool mouse_right_pressed;

void init_mouse(void);
void mouse_set_bounds(uint32_t width, uint32_t height);
void draw_cursor(int x, int y);

#endif