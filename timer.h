#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void init_timer(uint32_t frequency);
uint32_t get_uptime_seconds(void);
extern volatile uint32_t timer_ticks;

// ДОДАНО: Функція паузи
void sleep(uint32_t seconds);

#endif