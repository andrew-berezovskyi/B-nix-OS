#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

#define SHELL_BUFFER_SIZE 256
extern char command_buffer[SHELL_BUFFER_SIZE];
extern int buffer_index;

// Ініціалізація оболонки при старті ОС
void init_shell(void);

// Функція, яка буде приймати кожну натиснуту літеру з клавіатури
void shell_handle_keypress(char c);

#endif