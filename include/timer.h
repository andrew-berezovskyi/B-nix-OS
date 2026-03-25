#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>

void init_timer(uint32_t frequency);
uint32_t get_uptime_seconds(void);
extern volatile uint32_t timer_ticks;
void sleep(uint32_t seconds);

// ==============================================================
// MULTITASKING
// ==============================================================
typedef struct {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; 
    uint32_t eip;                                    
    uint32_t cs;                                     
    uint32_t eflags;                                 
} context_t;

// НОВЕ: Статуси задач (справжній сон)
#define TASK_FREE     0
#define TASK_RUNNING  1
#define TASK_SLEEPING 2

typedef struct {
    uint32_t id;          
    context_t context;    
    uint8_t* stack;       
    int state;           // TASK_FREE, TASK_RUNNING, TASK_SLEEPING
    uint32_t wake_time;  // Тік таймера, коли програму треба розбудити
} task_t;

#define MAX_TASKS 4

void init_multitasking(void);
int create_task(void (*entry_point)(void));
void schedule(context_t* current_context);

#endif