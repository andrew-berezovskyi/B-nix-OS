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

// Статуси задач
#define TASK_FREE         0
#define TASK_RUNNING      1
#define TASK_SLEEPING     2
#define TASK_WAITING_KBD  3  // Програма чекає вводу з клавіатури

typedef struct {
    uint32_t id;          
    context_t context;    
    uint8_t* stack;       
    int state;           
    uint32_t wake_time;  
    uint32_t* page_directory; // 🔥 НОВЕ: Паспорт віртуальної пам'яті процесу
} task_t;

#define MAX_TASKS 4

extern int current_task;
extern task_t tasks[MAX_TASKS];

void init_multitasking(void);
// 🔥 ЗМІНЕНО: Тепер ми передаємо каталог сторінок при створенні задачі
int create_task(void (*entry_point)(void), uint32_t* pagedir);
void schedule(context_t* current_context);
void exit_current_task(void); 
void wake_up_task(int task_id);

#endif