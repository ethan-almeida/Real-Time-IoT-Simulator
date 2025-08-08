/* Libraries */
#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

/* Function Definitions */
void vTask1(void *pvParameters);
void UART_Init(void);


int main(void) {

    UART_Init();
    xTaskCreate(vTask1, "Task1", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    vTaskStartScheduler();

    for(;;);
}

void vTask1(void *pvParameters) {
    const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;

    for(;;) {
        printf("Task 1 is running!\n");
        vTaskDelay(xDelay);
    }
}

void UART_Init(void) {
}