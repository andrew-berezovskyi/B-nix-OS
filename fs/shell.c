#include "shell.h"
#include "fs.h"
#include "sys_api.h"
#include "kheap.h"   
#include "timer.h"   
#include "elf.h"      
#include "vmm.h"      
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

volatile bool app_waiting_for_input = false;
char* app_input_buffer = 0;
bool suppress_prompt = false; 

int running_app_task_id = -1;

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
int strncmp(const char* s1, const char* s2, int n) {
    while (n && *s1 && (*s1 == *s2)) { ++s1; ++s2; --n; }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void print_prompt() { print("B-nix> "); }

// ========================================================
// 🔥 СПРАВЖНІЙ ELF LOADER (З ВИРІВНЮВАННЯМ!)
// ========================================================
static int load_elf_and_run(const char* filename) {
    int fd = sys_open(filename, O_RDONLY);
    if (fd == -1) {
        print("ELF Error: Cannot open file.\n");
        return -1;
    }

    elf32_ehdr_t ehdr;
    if (sys_read(fd, &ehdr, sizeof(elf32_ehdr_t)) != sizeof(elf32_ehdr_t)) {
        print("ELF Error: File too small.\n");
        sys_close(fd);
        return -1;
    }

    if (ehdr.e_ident[0] != ELFMAG0 || ehdr.e_ident[1] != ELFMAG1 || 
        ehdr.e_ident[2] != ELFMAG2 || ehdr.e_ident[3] != ELFMAG3) {
        print("ELF Error: Not an ELF file.\n");
        sys_close(fd);
        return -1;
    }

    uint32_t* pagedir = vmm_create_address_space();
    bool loaded_anything = false;

    int skip_to_phdr = ehdr.e_phoff - sizeof(elf32_ehdr_t);
    char tmp[64];
    while (skip_to_phdr > 0) {
        int r = skip_to_phdr > 64 ? 64 : skip_to_phdr;
        sys_read(fd, tmp, r);
        skip_to_phdr -= r;
    }

    for (int i = 0; i < ehdr.e_phnum; i++) {
        elf32_phdr_t phdr;
        if (sys_read(fd, &phdr, sizeof(elf32_phdr_t)) != sizeof(elf32_phdr_t)) break;

        if (phdr.p_type == PT_LOAD && phdr.p_memsz > 0) {
            loaded_anything = true;
            
            uint32_t offset_in_page = phdr.p_vaddr & 0xFFF;
            uint32_t pages_needed = (phdr.p_memsz + offset_in_page + 4095) / 4096;
            
            uint32_t raw_alloc = (uint32_t)kmalloc(pages_needed * 4096 + 4096);
            uint32_t phys_addr = (raw_alloc + 4095) & ~0xFFF;

            int seg_fd = sys_open(filename, O_RDONLY);
            int skip = phdr.p_offset;
            while (skip > 0) {
                int r = skip > 64 ? 64 : skip;
                sys_read(seg_fd, tmp, r);
                skip -= r;
            }
            
            sys_read(seg_fd, (void*)(phys_addr + offset_in_page), phdr.p_filesz);
            sys_close(seg_fd);

            if (phdr.p_memsz > phdr.p_filesz) {
                uint8_t* bss = (uint8_t*)(phys_addr + offset_in_page + phdr.p_filesz);
                for (uint32_t j = 0; j < (phdr.p_memsz - phdr.p_filesz); j++) bss[j] = 0;
            }

            for (uint32_t j = 0; j < pages_needed; j++) {
                vmm_map_page(pagedir, 
                             (phdr.p_vaddr & ~0xFFF) + (j * 4096), 
                             phys_addr + (j * 4096), 
                             PTE_USER | PTE_RW);
            }
        }
    }
    sys_close(fd);

    if (!loaded_anything) {
        print("ELF Error: No loadable segments.\n");
        return -1;
    }

    int task_id = create_task((void (*)(void))ehdr.e_entry, pagedir);
    if (task_id == -1) {
        print("ELF Error: Task limit reached (Max 4). Please reboot OS!\n");
    }
    return task_id;
}
// ========================================================

void execute_command() {
    command_buffer[buffer_index] = '\0';
    suppress_prompt = false; 

    if (buffer_index == 0) { } 
    else if (strcmp(command_buffer, "help") == 0) {
        print("Available commands:\n  ls, cat, rm, echo, clear, info, uptime, meminfo, sleep, run\n");
    } 
    else if (strcmp(command_buffer, "clear") == 0) { terminal_clear(); } 
    else if (strcmp(command_buffer, "ls") == 0) {
        int dir_fd = fs_opendir("/");
        if (dir_fd != -1) {
            fs_dirent_t entry; int count = 0;
            while (fs_readdir(dir_fd, &entry)) {
                if (entry.type == FS_TYPE_DIR) print("[DIR]  "); else print("[FILE] ");
                print(entry.name); print("\n"); count++;
            }
            fs_closedir(dir_fd);
            if (count == 0) print("Directory is empty.\n");
        }
    }
    else if (strncmp(command_buffer, "cat ", 4) == 0) {
        char* filename = command_buffer + 4; while (*filename == ' ') filename++; 
        if (*filename != '\0') {
            int fd = sys_open(filename, O_RDONLY);
            if (fd != -1) {
                char buf[128]; int bytes_read;
                while ((bytes_read = sys_read(fd, buf, 127)) > 0) { buf[bytes_read] = '\0'; print(buf); }
                print("\n"); sys_close(fd);
            } else print("No such file\n");
        }
    }
    else if (strncmp(command_buffer, "run ", 4) == 0) {
        char* filename = command_buffer + 4; while (*filename == ' ') filename++;
        int len = 0; while (filename[len]) len++;
        while (len > 0 && filename[len-1] == ' ') { filename[len-1] = '\0'; len--; }
        if (*filename != '\0') {
            running_app_task_id = load_elf_and_run(filename);
            if (running_app_task_id != -1) {
                suppress_prompt = true;
            }
        }
    }
    else if (strncmp(command_buffer, "rm ", 3) == 0) {
        char* filename = command_buffer + 3; while (*filename == ' ') filename++;
        int len = 0; while (filename[len]) len++;
        while (len > 0 && filename[len-1] == ' ') { filename[len-1] = '\0'; len--; }
        if (*filename != '\0') { if (sys_unlink(filename) == 0) print("File deleted.\n"); else print("Error\n"); }
    }
    else { print("Unknown command.\n"); }
    buffer_index = 0;
}

void shell_readline(char* out_buf) {
    app_input_buffer = out_buf;
    buffer_index = 0; 
    app_waiting_for_input = true;
    
    asm volatile("cli");
    tasks[current_task].state = TASK_WAITING_KBD;
    asm volatile("sti"); 
    
    while (tasks[current_task].state == TASK_WAITING_KBD) {
        asm volatile("hlt");
    }
}

void shell_handle_keypress(char c) {
    if (app_waiting_for_input) {
        if (c == '\n') {
            print("\n");
            command_buffer[buffer_index] = '\0';
            
            char* dest = app_input_buffer; char* src = command_buffer;
            while(*src) *dest++ = *src++;
            *dest = '\0';
            
            buffer_index = 0; 
            app_waiting_for_input = false; 
            
            if (running_app_task_id != -1) {
                wake_up_task(running_app_task_id);
            }
        } else if (c == '\b') {
            if (buffer_index > 0) { buffer_index--; terminal_putchar('\b'); }
        } else if (buffer_index < SHELL_BUFFER_SIZE - 1) { 
            command_buffer[buffer_index++] = c; terminal_putchar(c); 
        }
        return; 
    }

    if (c == '\n') { 
        print("\n"); execute_command(); 
        if (!suppress_prompt) print_prompt(); 
    } else if (c == '\b') {
        if (buffer_index > 0) { buffer_index--; terminal_putchar('\b'); }
    } else if (buffer_index < SHELL_BUFFER_SIZE - 1) { 
        command_buffer[buffer_index++] = c; terminal_putchar(c); 
    }
}

void init_shell() { buffer_index = 0; print("\n"); print_prompt(); }