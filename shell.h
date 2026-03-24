#ifndef SHELL_H
#define SHELL_H

// Ініціалізація оболонки при старті ОС
void init_shell(void);

// Функція, яка буде приймати кожну натиснуту літеру з клавіатури
void shell_handle_keypress(char c);

#endif