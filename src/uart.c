#include "uart.h"

// Correct UART addresses for versatilepb
#define UART0_BASE 0x101f1000
#define UART_DR    (*(volatile unsigned int*)(UART0_BASE + 0x00))
#define UART_FR    (*(volatile unsigned int*)(UART0_BASE + 0x18))

void uart_init(void) {
    // For versatilepb, UART is already initialized by QEMU
    // No additional setup needed
}

void uart_putc(char c) {
    // Wait for UART to be ready (TX FIFO not full)
    while (UART_FR & (1 << 5));
    UART_DR = c;
}

void uart_puts(const char* str) {
    while (*str) {
        uart_putc(*str++);
    }
}