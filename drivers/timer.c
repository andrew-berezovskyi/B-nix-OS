#include "timer.h"
#include "io.h"
#include "vmm.h" // 🔥 НОВЕ: Для перемикання пам'яті

volatile uint32_t timer_ticks = 0;
uint32_t current_frequency = 0;

task_t tasks[MAX_TASKS];
int current_task = -1;
bool multitasking_enabled = false;

static uint8_t task_stacks[MAX_TASKS][32768]; 

void init_multitasking(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_FREE;
        tasks[i].id = i;
    }
    tasks[0].state = TASK_RUNNING;
    // Головна задача (ядро/GUI) використовує глобальний каталог ядра
    tasks[0].page_directory = kernel_directory; 
    current_task = 0;
    multitasking_enabled = true;
}

// 🔥 ОНОВЛЕНО: Приймаємо page_directory
int create_task(void (*entry_point)(void), uint32_t* pagedir) {
    asm volatile("cli");
    int new_id = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_FREE) { new_id = i; break; }
    }
    if (new_id == -1) { asm volatile("sti"); return -1; }

    uint32_t* stack = (uint32_t*)(task_stacks[new_id] + 32768);

    *(--stack) = 0x202;        // EFLAGS
    *(--stack) = 0x08;         // CS
    *(--stack) = (uint32_t)entry_point; // EIP

    *(--stack) = 0; // EAX
    *(--stack) = 0; // ECX
    *(--stack) = 0; // EDX
    *(--stack) = 0; // EBX
    *(--stack) = (uint32_t)stack; // ESP
    *(--stack) = 0; // EBP
    *(--stack) = 0; // ESI
    *(--stack) = 0; // EDI

    tasks[new_id].context.esp = (uint32_t)stack;
    // Прив'язуємо простір до задачі (якщо NULL, то це потік ядра)
    tasks[new_id].page_directory = pagedir ? pagedir : kernel_directory;
    tasks[new_id].state = TASK_RUNNING;
    
    asm volatile("sti");
    return new_id;
}

uint32_t timer_handler_main(uint32_t current_esp) {
    timer_ticks++;

    if (multitasking_enabled) {
        for (int i = 0; i < MAX_TASKS; i++) {
            if (tasks[i].state == TASK_SLEEPING && timer_ticks >= tasks[i].wake_time) {
                tasks[i].state = TASK_RUNNING;
            }
        }

        tasks[current_task].context.esp = current_esp;
        int next_task = current_task;
        
        do {
            next_task = (next_task + 1) % MAX_TASKS;
        } while (tasks[next_task].state != TASK_RUNNING);

        current_task = next_task;
        current_esp = tasks[current_task].context.esp;

        // 🔥 МАГІЯ ЛІНУКСА: Перемикаємо Віртуальну Пам'ять (CR3) для нової задачі!
        vmm_switch_directory(tasks[current_task].page_directory);
    }

    outb(0x20, 0x20);
    return current_esp;
}

void wake_up_task(int task_id) {
    if (task_id >= 0 && task_id < MAX_TASKS) {
        if (tasks[task_id].state == TASK_WAITING_KBD) {
            tasks[task_id].state = TASK_RUNNING;
        }
    }
}

void exit_current_task(void) {
    asm volatile("cli");
    if (current_task > 0 && current_task < MAX_TASKS) {
        tasks[current_task].state = TASK_FREE; 
        // В ідеалі тут треба ще робити kfree() для page_directory, але поки що залишимо так
    }
    asm volatile("sti");
    while(1) { asm volatile("hlt"); } 
}

void init_timer(uint32_t frequency) {
    current_frequency = frequency;
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t get_uptime_seconds(void) { return timer_ticks / current_frequency; }

void sleep(uint32_t seconds) {
    if (multitasking_enabled && current_task != -1) {
        tasks[current_task].wake_time = timer_ticks + (seconds * current_frequency);
        tasks[current_task].state = TASK_SLEEPING;
        while (tasks[current_task].state == TASK_SLEEPING) {
            asm volatile("hlt");
        }
    } else {
        uint32_t start = timer_ticks;
        uint32_t wait = seconds * current_frequency;
        while ((timer_ticks - start) < wait) { asm volatile("hlt"); }
    }
}