#include "uart.h"

int main(void) {
    uart_init();
    
    uart_puts("=== IoT Edge Device Simulator ===\r\n");
    uart_puts("Phase 1: Basic System Operational\r\n");
    
    int counter = 0;
    
    while (1) {
        uart_puts("Heartbeat: ");
        uart_putc('0' + (counter % 10));
        uart_puts("\r\n");
        
        counter++;
        if (counter >= 10) {
            counter = 0;
        }
    }
    
    return 0;
}