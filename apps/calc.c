#include "libc.h"

static void print_result(int a, char op, int b, int result) {
    char out[32];
    sys_print("\n  ");
    itoa(a, out); sys_print(out);
    sys_print(" ");
    out[0] = op; out[1] = '\0'; sys_print(out);
    sys_print(" ");
    itoa(b, out); sys_print(out);
    sys_print(" = ");
    itoa(result, out); sys_print(out);
    sys_print("\n\n");
}

static bool calculate(int a, char op, int b, int* result) {
    if (!result) return false;
    if (op == '+') *result = a + b;
    else if (op == '-') *result = a - b;
    else if (op == '*') *result = a * b;
    else if (op == '/') {
        if (b == 0) return false;
        *result = a / b;
    } else if (op == '%') {
        if (b == 0) return false;
        *result = a % b;
    } else return false;
    return true;
}

/*
 * Aurora Calculator is linked as its own ELF32 image. The kernel only provides
 * the small console syscall ABI, so the same binary can be copied onto a B-nix
 * disk and launched without rebuilding the kernel.
 */
void __attribute__((section(".entry"))) _start(void) {
    char input[64];
    char operation[16];

    sys_print("\n+----------------------------------+\n");
    sys_print("|        AURORA CALCULATOR         |\n");
    sys_print("+----------------------------------+\n");
    sys_print("| +  -  *  /  %        q = quit   |\n");
    sys_print("+----------------------------------+\n\n");

    for (;;) {
        get_input("First number (or q): ", input);
        if (strcmp(input, "q") == 0 || strcmp(input, "quit") == 0) break;
        int a = atoi(input);

        get_input("Operation: ", operation);
        if (strcmp(operation, "q") == 0 || strcmp(operation, "quit") == 0) break;

        get_input("Second number: ", input);
        int b = atoi(input);
        int result = 0;

        if (!calculate(a, operation[0], b, &result)) {
            if ((operation[0] == '/' || operation[0] == '%') && b == 0)
                sys_print("\nCannot divide by zero. Try again.\n\n");
            else
                sys_print("\nUnknown operation. Use +, -, *, / or %.\n\n");
            continue;
        }
        print_result(a, operation[0], b, result);
    }

    sys_print("\nCalculator closed.\n\nB-nix> ");
    sys_exit();
}
