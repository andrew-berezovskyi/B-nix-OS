#ifndef MOUSE_H
#define MOUSE_H

#include <stdbool.h>
#include <stdint.h>

// Глобальні змінні, які ми експортуємо для ядра
extern int mouse_x;
extern int mouse_y;
extern bool mouse_left_pressed;

void init_mouse(void);
void draw_cursor(int x, int y);

#endif