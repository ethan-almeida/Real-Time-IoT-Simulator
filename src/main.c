#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "FreeRTOS.h"
#include "task.h"

// Task function prototypes
void vTask1(void *pvParameters);
void vTask2(void *pvParameters);
void vTask3(void *pvParameters);

int main(void) {
    xTaskCreate(vTask1, "Task1", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vTask2, "Task2", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(vTask3, "Task3", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
    vTaskStartScheduler();
    
    return -1;
}

void vTask1(void *pvParameters) {
    for (;;) {
        printf("Task 1 running\n");
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

void vTask2(void *pvParameters) {
    for (;;) {
        printf("Task 2 running\n");
        vTaskDelay(pdMS_TO_TICKS(1500)); 
    }
}

void vTask3(void *pvParameters) {
    for (;;) {
        printf("Task 3 running\n");
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

void vApplicationMallocFailedHook(void) {
    printf("Malloc failed!\n");
    configASSERT(0);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    printf("Stack overflow in task %s!\n", pcTaskName);
    configASSERT(0);
}