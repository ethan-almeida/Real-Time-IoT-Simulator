#include "uart.h"

#define UART0_BASE 0x101f1000
#define UART_DR    (*(volatile unsigned int*)(UART0_BASE + 0x00))
#define UART_FR    (*(volatile unsigned int*)(UART0_BASE + 0x18))

void uart_init(void) {
}

void uart_putc(char c) {
    while (UART_FR & (1 << 5));
    UART_DR = c;
}

void uart_puts(const char* str) {
    while (*str) {
        uart_putc(*str++);
    }
}