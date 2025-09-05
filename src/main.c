#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "config.h"
#include "common.h"
#include "tsk_priority.h"

QueueHandle_t xSensorQueue = NULL;
SemaphoreHandle_t xConsoleMutex = NULL;
EventGroupHandle_t xSystemEvents = NULL;

extern void vTemperatureSensorTask(void *pvParameters);
extern void vHumiditySensorTask(void *pvParameters);
extern void vMotionSensorTask(void *pvParameters);
void vDataProcessorTask(void *pvParameters);
void vSystemMonitorTask(void *pvParameters);
void safe_printf(const char *format, ...);
uint32_t get_system_time_ms(void);

int main(void) {
    printf("Starting IoT Gateway \n");
    srand(time(NULL));

    xSensorQueue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(sensor_data_t));
    if (xSensorQueue == NULL) {
        printf("Error: Failed to create sensor queue!\n");
        return -1;
    }
    
    xConsoleMutex = xSemaphoreCreateMutex();
    if (xConsoleMutex == NULL) {
        printf("Error: Failed to create console mutex!\n");
        return -1;
    }
    
    xSystemEvents = xEventGroupCreate();
    if (xSystemEvents == NULL) {
        printf("Error: Failed to create event group!\n");
        return -1;
    }
    
    BaseType_t xReturned;
    
    for (int i = 0; i < NUM_TEMP_SENSORS; i++) {
        char taskName[32];
        snprintf(taskName, sizeof(taskName), "TempSensor%d", i);
        
        xReturned = xTaskCreate(
            vTemperatureSensorTask,
            taskName,
            SENSOR_TASK_STACK_SIZE,
            (void*)(intptr_t)i,
            PRIORITY_SENSOR_LOW,
            NULL
        );
        if (xReturned != pdPASS) {
            printf("Error: Failed to create temperature sensor task %d\n", i);
            return -1;
        }
    }

    for (int i = 0; i < NUM_HUMIDITY_SENSORS; i++) {
        char taskName[32];
        snprintf(taskName, sizeof(taskName), "HumidSensor%d", i);
        
        xReturned = xTaskCreate(
            vHumiditySensorTask,
            taskName,
            SENSOR_TASK_STACK_SIZE,
            (void*)(intptr_t)i,
            PRIORITY_SENSOR_LOW,
            NULL
        );
        if (xReturned != pdPASS) {
            printf("Error: Failed to create humidity sensor task %d\n", i);
            return -1;
        }
    }
    
    xReturned = xTaskCreate(
        vMotionSensorTask,
        "MotionSensor",
        SENSOR_TASK_STACK_SIZE,
        NULL,
        PRIORITY_SENSOR_HIGH,
        NULL
    );
    if (xReturned != pdPASS) {
        printf("Error: Failed to create motion sensor task\n");
        return -1;
    }
    
    xReturned = xTaskCreate(
        vDataProcessorTask,
        "DataProcessor",
        PROCESSOR_TASK_STACK_SIZE,
        NULL,
        PRIORITY_PROCESSOR,
        NULL
    );
    if (xReturned != pdPASS) {
        printf("Error: Failed to create data processor task\n");
        return -1;
    }
    
    xReturned = xTaskCreate(
        vSystemMonitorTask,
        "SystemMonitor",
        MONITOR_TASK_STACK_SIZE,
        NULL,
        PRIORITY_MONITOR,
        NULL
    );
    if (xReturned != pdPASS) {
        printf("Error: Failed to create system monitor task\n");
        return -1;
    }
    printf("Starting scheduler...\n");
    vTaskStartScheduler();
    printf("Error: Scheduler returned!\n");
    return -1;
}

void vDataProcessorTask(void *pvParameters) {
    sensor_data_t sensor_data;
    
    safe_printf("[DataProcessor] Started\n");
    
    for (;;) {
        if (xQueueReceive(xSensorQueue, &sensor_data, pdMS_TO_TICKS(1000)) == pdPASS) {
            const char* sensor_type_str;
            switch (sensor_data.type) {
                case SENSOR_TYPE_TEMPERATURE:
                    sensor_type_str = "Temperature";
                    break;
                case SENSOR_TYPE_HUMIDITY:
                    sensor_type_str = "Humidity";
                    break;
                case SENSOR_TYPE_MOTION:
                    sensor_type_str = "Motion";
                    break;
                default:
                    sensor_type_str = "Unknown";
                    break;
            }
            safe_printf("[DataProcessor] Processing %s sensor %d: %.2f\n",
                       sensor_type_str, sensor_data.sensor_id, sensor_data.value);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void vSystemMonitorTask(void *pvParameters) {
    safe_printf("[SystemMonitor] Started\n");
    for (;;) {
        UBaseType_t uxSensorQueueMessages = uxQueueMessagesWaiting(xSensorQueue);
        safe_printf("[SystemMonitor] Sensor queue has %lu messages\n", 
                   (unsigned long)uxSensorQueueMessages);
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}


void safe_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdTRUE) {
        vprintf(format, args);
        fflush(stdout);
        xSemaphoreGive(xConsoleMutex);
    }
    va_end(args);
}

uint32_t get_system_time_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void vApplicationMallocFailedHook(void) {
    printf("Malloc failed!\n");
    configASSERT(0);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    printf("Stack overflow in task %s!\n", pcTaskName);
    configASSERT(0);
}