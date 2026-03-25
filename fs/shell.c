#include "shell.h"
#include "fs.h"
#include "sys_api.h"
#include <stdint.h>

extern void print(const char* str);
extern void terminal_putchar(char c);
extern void terminal_clear(void);
extern uint32_t get_uptime_seconds(void); 
extern void sleep(uint32_t seconds); 

extern uint32_t pmm_get_free_block_count(void);
extern uint32_t pmm_get_max_blocks(void);

char command_buffer[SHELL_BUFFER_SIZE];
int buffer_index = 0;

// Допоміжні функції
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n && *s1 && (*s1 == *s2)) { ++s1; ++s2; --n; }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int atoi(const char* str) {
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i) {
        if (str[i] >= '0' && str[i] <= '9') res = res * 10 + str[i] - '0';
        else break; 
    }
    return res;
}

void itoa(uint32_t num, char* str) {
    int i = 0;
    if (num == 0) { str[i++] = '0'; str[i] = '\0'; return; }
    while (num != 0) { 
        int rem = num % 10; 
        str[i++] = rem + '0'; 
        num = num / 10; 
    }
    str[i] = '\0';
    int start = 0; int end = i - 1;
    while (start < end) { 
        char temp = str[start]; 
        str[start] = str[end]; 
        str[end] = temp; 
        start++; end--; 
    }
}

void print_prompt() { 
    print("B-nix> "); 
}

// Головний обробник команд
void execute_command() {
    command_buffer[buffer_index] = '\0';

    if (buffer_index == 0) { } 
    else if (strcmp(command_buffer, "help") == 0) {
        print("Available commands:\n");
        print("  ls             - List files and directories\n");   
        print("  cat <file>     - Print file content\n");           
        print("  rm <file>      - Delete a file\n");                
        print("  echo <txt> > <file> - Write text to file\n");      
        print("  clear          - Clear the screen\n");
        print("  info           - Show system information\n");
        print("  uptime         - Show system uptime\n");
        print("  meminfo        - Show physical memory status\n");
        print("  sleep <sec>    - Pause system for X seconds\n");
    } 
    else if (strcmp(command_buffer, "info") == 0) {
        print("B-nix OS v0.1\n");
    } 
    else if (strcmp(command_buffer, "clear") == 0) {
        terminal_clear();
    } 
    else if (strcmp(command_buffer, "ls") == 0) {
        int dir_fd = fs_opendir("/");
        if (dir_fd != -1) {
            fs_dirent_t entry; int count = 0;
            while (fs_readdir(dir_fd, &entry)) {
                if (entry.type == FS_TYPE_DIR) print("[DIR]  "); 
                else print("[FILE] ");
                print(entry.name); print("\n"); count++;
            }
            fs_closedir(dir_fd);
            if (count == 0) print("Directory is empty.\n");
        }
    }
    else if (strncmp(command_buffer, "cat ", 4) == 0) {
        const char* filename = command_buffer + 4;
        while (*filename == ' ') filename++; 
        
        if (*filename == '\0') { print("Usage: cat <filename>\n"); } 
        else {
            int fd = sys_open(filename, O_RDONLY);
            if (fd != -1) {
                char buf[128]; int bytes_read;
                while ((bytes_read = sys_read(fd, buf, sizeof(buf) - 1)) > 0) {
                    buf[bytes_read] = '\0'; print(buf);
                }
                print("\n"); sys_close(fd);
            } else {
                print("cat: "); print(filename); print(": No such file\n");
            }
        }
    }
    // ========================================================
    // ОНОВЛЕНИЙ 'rm': Тепер з відрізанням зайвих пробілів!
    // ========================================================
    else if (strncmp(command_buffer, "rm ", 3) == 0) {
        char* filename = command_buffer + 3; // Змінили const char на char
        while (*filename == ' ') filename++;
        
        // Відрізаємо пробіли в кінці імені
        int len = 0; 
        while (filename[len]) len++;
        while (len > 0 && filename[len-1] == ' ') { filename[len-1] = '\0'; len--; }
        
        if (*filename == '\0') { print("Usage: rm <filename>\n"); } 
        else {
            if (sys_unlink(filename) == 0) {
                print("File deleted.\n");
            } else {
                print("rm: cannot remove '"); print(filename); print("'\n");
            }
        }
    }
    else if (strncmp(command_buffer, "echo ", 5) == 0) {
        char* cmd = command_buffer + 5; char* redirect = 0;
        for (int i = 0; cmd[i] != '\0'; i++) {
            if (cmd[i] == '>') { redirect = &cmd[i]; break; }
        }
        if (redirect) {
            *redirect = '\0'; 
            char* text = cmd; char* file = redirect + 1;
            while (*file == ' ') file++; while (*text == ' ') text++;
            
            int len = 0; while (text[len]) len++;
            while (len > 0 && text[len-1] == ' ') { text[len-1] = '\0'; len--; }

            int fd = sys_open(file, O_CREAT | O_WRONLY); 
            if (fd != -1) {
                sys_write(fd, text, len); sys_close(fd);
                print("Saved to "); print(file); print("\n");
            } else { print("Error opening file.\n"); }
        } else {
            print(cmd); print("\n");
        }
    }
    else if (strcmp(command_buffer, "uptime") == 0) {
        uint32_t uptime = get_uptime_seconds(); char uptime_str[16];
        itoa(uptime, uptime_str);
        print("System is running for: "); print(uptime_str); print(" seconds\n");
    }
    else if (strcmp(command_buffer, "meminfo") == 0) {
        uint32_t free_blocks = pmm_get_free_block_count();
        uint32_t max_blocks = pmm_get_max_blocks(); 
        uint32_t used_blocks = max_blocks - free_blocks;

        char free_str[16], max_str[16], used_str[16];
        itoa(free_blocks, free_str); itoa(max_blocks, max_str); itoa(used_blocks, used_str);

        print("Total blocks: "); print(max_str); print("\n");
        print("Used blocks : "); print(used_str); print("\n");
        print("Free blocks : "); print(free_str); print("\n");
    }
    else if (strncmp(command_buffer, "sleep ", 6) == 0) {
        int secs = atoi(command_buffer + 6);
        if (secs > 0) {
            print("Sleeping...\n"); sleep(secs);
        }
    }
    else { print("Unknown command. Type 'help'\n"); }
    
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
        if (buffer_index < SHELL_BUFFER_SIZE - 1) { 
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