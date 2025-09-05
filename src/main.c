#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

QueueHandle_t xQueue;
SemaphoreHandle_t xResourceSemaphore;

void vSenderTask(void *pvParameters);
void vReceiverTask(void *pvParameters);
void vResourceUserTask1(void *pvParameters);
void vResourceUserTask2(void *pvParameters);
void access_shared_resource(const char* taskName, int taskId);

int main(void) {
    xQueue = xQueueCreate(5, sizeof(uint32_t));
    if (xQueue == NULL) {
        printf("Error: Failed to create queue!\n");
        return -1;
    }

    xResourceSemaphore = xSemaphoreCreateMutex();
    if (xResourceSemaphore == NULL) {
        printf("Error: Failed to create semaphore!\n");
        return -1;
    }

    xTaskCreate(vSenderTask, "Sender", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(vReceiverTask, "Receiver", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vResourceUserTask1, "ResourceUser1", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
    xTaskCreate(vResourceUserTask2, "ResourceUser2", configMINIMAL_STACK_SIZE, NULL, 3, NULL);

    vTaskStartScheduler();
    return -1;
}

void vSenderTask(void *pvParameters) {
    uint32_t ulValueToSend = 0;
    
    for (;;) {
        ulValueToSend++;
        if (xQueueSend(xQueue, &ulValueToSend, pdMS_TO_TICKS(100)) != pdTRUE) {
            access_shared_resource("Sender", 1);
            printf("Sender: Failed to send value %u to queue\n", (unsigned int)ulValueToSend);
        } else {
            access_shared_resource("Sender", 1);
            printf("Sender: Sent value %u to queue\n", (unsigned int)ulValueToSend);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vReceiverTask(void *pvParameters) {
    uint32_t ulReceivedValue;
    
    for (;;) {
        if (xQueueReceive(xQueue, &ulReceivedValue, pdMS_TO_TICKS(500)) == pdTRUE) {
            access_shared_resource("Receiver", 2);
            printf("Receiver: Received value %u from queue\n", (unsigned int)ulReceivedValue);
        } else {
            access_shared_resource("Receiver", 2);
            printf("Receiver: No data available on queue\n");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vResourceUserTask1(void *pvParameters) {
    for (;;) {
        access_shared_resource("ResourceUser1", 3);
        printf("ResourceUser1: Accessed shared resource\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void vResourceUserTask2(void *pvParameters) {
    for (;;) {
        access_shared_resource("ResourceUser2", 4);
        printf("ResourceUser2: Accessed shared resource\n");
        vTaskDelay(pdMS_TO_TICKS(1500));
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

void access_shared_resource(const char* taskName, int taskId) {
    if (xSemaphoreTake(xResourceSemaphore, portMAX_DELAY) == pdTRUE) {
        printf("[%s-%d] Entering critical section\n", taskName, taskId);
        vTaskDelay(pdMS_TO_TICKS(100));
        printf("[%s-%d] Leaving critical section\n", taskName, taskId);
        xSemaphoreGive(xResourceSemaphore);
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