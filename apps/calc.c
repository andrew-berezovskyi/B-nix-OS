#include "libc.h"

// Точка входу прибита до початку
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