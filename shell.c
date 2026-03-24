#include "shell.h"
#include <stdint.h>

extern void print(const char* str);
extern void terminal_putchar(char c);
extern void terminal_clear(void);
extern uint32_t get_uptime_seconds(void); 
extern void sleep(uint32_t seconds); 

// ДОДАНО: Підключаємо функції з нашого PMM
extern uint32_t pmm_get_free_block_count(void);
extern uint32_t pmm_get_max_blocks(void); // Нам треба буде додати цю функцію в pmm.c!

#define BUFFER_SIZE 256
char command_buffer[BUFFER_SIZE];
int buffer_index = 0;

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n && *s1 && (*s1 == *s2)) {
        ++s1;
        ++s2;
        --n;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int atoi(const char* str) {
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i) {
        if (str[i] >= '0' && str[i] <= '9') {
            res = res * 10 + str[i] - '0';
        } else {
            break; 
        }
    }
    return res;
}

void itoa(uint32_t num, char* str) {
    int i = 0;
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    while (num != 0) {
        int rem = num % 10;
        str[i++] = rem + '0';
        num = num / 10;
    }
    str[i] = '\0';
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

void print_prompt() {
    print("B-nix> ");
}

void execute_command() {
    command_buffer[buffer_index] = '\0';

    if (buffer_index == 0) {
        // Порожній Enter
    } 
    else if (strcmp(command_buffer, "help") == 0) {
        print("Available commands:\n");
        print("  help           - Show this menu\n");
        print("  info           - Show system information\n");
        print("  clear          - Clear the screen\n");
        print("  uptime         - Show system uptime\n");
        print("  meminfo        - Show physical memory status\n"); // Оновили help
        print("  sleep <sec>    - Pause system for X seconds\n"); 
    } 
    else if (strcmp(command_buffer, "info") == 0) {
        print("B-nix OS v0.1\n");
        print("Architecture: x86 32-bit Protected Mode\n");
        print("Developer: Berezovskyi\n");
    } 
    else if (strcmp(command_buffer, "clear") == 0) {
        terminal_clear();
    } 
    else if (strcmp(command_buffer, "uptime") == 0) {
        uint32_t uptime = get_uptime_seconds();
        char uptime_str[16];
        itoa(uptime, uptime_str);
        print("System is running for: ");
        print(uptime_str);
        print(" seconds\n");
    }
    // ДОДАНО: Логіка команди meminfo
    else if (strcmp(command_buffer, "meminfo") == 0) {
        uint32_t free_blocks = pmm_get_free_block_count();
        // ВАЖЛИВО: Нам треба буде додати функцію pmm_get_max_blocks() в pmm.c
        uint32_t max_blocks = pmm_get_max_blocks(); 
        uint32_t used_blocks = max_blocks - free_blocks;

        char free_str[16], max_str[16], used_str[16];
        itoa(free_blocks, free_str);
        itoa(max_blocks, max_str);
        itoa(used_blocks, used_str);

        print("--- Physical Memory Status ---\n");
        print("Block size: 4 KB (4096 bytes)\n");
        
        print("Total blocks: ");
        print(max_str);
        print("\n");

        print("Used blocks : ");
        print(used_str);
        print("\n");

        print("Free blocks : ");
        print(free_str);
        print("\n");
        print("------------------------------\n");
    }
    else if (strncmp(command_buffer, "sleep ", 6) == 0) {
        int secs = atoi(command_buffer + 6);
        if (secs > 0) {
            print("Sleeping for ");
            char sec_str[16];
            itoa(secs, sec_str);
            print(sec_str);
            print(" seconds...\n");
            
            sleep(secs);
        } else {
            print("Error: Please provide a valid number of seconds (e.g., 'sleep 3')\n");
        }
    }
    else {
        print("Unknown command: ");
        print(command_buffer);
        print("\n");
    }

    buffer_index = 0;
}

void shell_handle_keypress(char c) {
    if (c == '\n') {
        print("\n");
        execute_command();
        print_prompt();
    } 
    else if (c == '\b') {
        if (buffer_index > 0) {
            buffer_index--;
            terminal_putchar('\b');
        }
    } 
    else {
        if (buffer_index < BUFFER_SIZE - 1) {
            command_buffer[buffer_index++] = c;
            terminal_putchar(c);
        }
    }
}

void init_shell() {
    buffer_index = 0;
    print("\n");
    print_prompt();
}