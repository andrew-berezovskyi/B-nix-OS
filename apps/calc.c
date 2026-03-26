void sys_print(const char* str);
void sys_readline(char* buf);
void sys_exit();
void itoa(int num, char* str);
int atoi(const char* str);

// Блокуємо порожні вводи
void get_input(const char* prompt, char* buf) {
    buf[0] = '\0';
    while (buf[0] == '\0') {
        sys_print(prompt);
        sys_readline(buf);
    }
}

void __attribute__((section(".entry"))) _start() {
    char input_buf[64];
    char out_buf[32];
    
    sys_print("\n==================================\n");
    sys_print(" B-NIX SMART CALCULATOR\n");
    sys_print("==================================\n");
    
    get_input("Enter first number: ", input_buf);
    int a = atoi(input_buf);
    
    get_input("Enter operation (+, -, *, /): ", input_buf);
    char op = input_buf[0];
    
    get_input("Enter second number: ", input_buf);
    int b = atoi(input_buf);
    
    int result = 0;
    if (op == '+') result = a + b;
    else if (op == '-') result = a - b;
    else if (op == '*') result = a * b;
    else if (op == '/') {
        if (b == 0) { sys_print("Error: Division by zero!\n\nB-nix> "); sys_exit(); }
        result = a / b;
    } else {
        sys_print("Error: Unknown operation!\n\nB-nix> "); sys_exit();
    }
    
    sys_print("\n>>> RESULT: ");
    itoa(a, out_buf); sys_print(out_buf); sys_print(" ");
    out_buf[0] = op; out_buf[1] = '\0'; sys_print(out_buf); sys_print(" ");
    itoa(b, out_buf); sys_print(out_buf); sys_print(" = ");
    itoa(result, out_buf); sys_print(out_buf); sys_print(" <<<\n");
    
    sys_print("\nB-nix> ");
    sys_exit();
}

void sys_print(const char* str) { asm volatile("mov $6, %%eax \n mov %0, %%ebx \n int $0x80" : : "r"(str) : "eax", "ebx"); }
void sys_readline(char* buf) { asm volatile("mov $8, %%eax \n mov %0, %%ebx \n int $0x80" : : "r"(buf) : "eax", "ebx", "memory"); }
void sys_exit() { asm volatile("mov $7, %%eax \n int $0x80" : : : "eax"); }
void itoa(int num, char* str) {
    int i = 0; if (num == 0) { str[i++] = '0'; str[i] = '\0'; return; }
    if (num < 0) { str[i++] = '-'; num = -num; }
    int temp = num; int length = 0; while (temp != 0) { length++; temp /= 10; }
    for (int j = length - 1; j >= 0; j--) { str[i + j] = (num % 10) + '0'; num /= 10; }
    str[i + length] = '\0';
}
int atoi(const char* str) {
    int res = 0; int sign = 1; int i = 0;
    if (str[0] == '-') { sign = -1; i++; }
    for (; str[i] != '\0'; ++i) { if (str[i] >= '0' && str[i] <= '9') res = res * 10 + str[i] - '0'; else break; }
    return sign * res;
}