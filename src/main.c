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
#include "FreeRTOSConfig.h"

QueueHandle_t xSensorQueue = NULL;
QueueHandle_t xNetworkQueue = NULL;
SemaphoreHandle_t xConsoleMutex = NULL;
EventGroupHandle_t xSystemEvents = NULL;

extern void vTemperatureSensorTask(void *pvParameters);
extern void vHumiditySensorTask(void *pvParameters);
extern void vMotionSensorTask(void *pvParameters);
extern void vNetworkTask(void *pvParameters);
extern void vSecurityTask(void *pvParameters);
void vDataProcessorTask(void *pvParameters);
void vSystemMonitorTask(void *pvParameters);
void safe_printf(const char *format, ...);
uint32_t get_system_time_ms(void);


int main(void) {
    printf("Starting IoT Gateway \n");
    srand(time(NULL));

    printf("Creating sensor queue...\n");  
    xSensorQueue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(sensor_data_t));
    if (xSensorQueue == NULL) {
        printf("Error: Failed to create sensor queue!\n");
        return -1;
    }
    printf("Sensor queue created\n");  
    

    printf("Creating console mutex...\n");  
    xConsoleMutex = xSemaphoreCreateMutex();
    if (xConsoleMutex == NULL) {
        printf("Error: Failed to create console mutex!\n");
        return -1;
    }
    printf("Console mutex created\n");  
    


    printf("Creating system events...\n");  
    xSystemEvents = xEventGroupCreate();
    if (xSystemEvents == NULL) {
        printf("Error: Failed to create event group!\n");
        return -1;
    }
    printf("System events created\n");  



    printf("Creating network queue...\n");  
    xNetworkQueue = xQueueCreate(NETWORK_QUEUE_LENGTH, sizeof(message_t));
    if (xNetworkQueue == NULL) {
        printf("Error: Failed to create network queue!\n");
        return -1;
    }
    printf("Network queue created\n");
    

    printf("About to create tasks...\n");  
    BaseType_t xReturned;
    

    /* Data Processor Task */
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
    } else {
    printf("DataProcessor task created successfully\n");  
    }
    

    /* Network Task */
    printf("Creating network task\n");
    printf("about to call task create for network task\n");
    xReturned = xTaskCreate(
        vNetworkTask,
        "Network",
        NETWORK_TASK_STACK_SIZE,
        NULL,
        0,
        NULL
    );
    printf("xTaskCreate returned: %d\n", xReturned); 
    if (xReturned != pdPASS) {
    printf("Error: Failed to create network task\n");
    return -1;
    } else {
    printf("Network task created successfully\n");  
    }
    

    /* Security Task */
    printf("creating security task\n");
    xReturned = xTaskCreate(
        vSecurityTask,
        "Security",
        128,
        NULL,
        2,
        NULL
    );
    printf("Security task xTaskCreate returned: %d\n", xReturned);  
    if (xReturned != pdPASS) {
        printf("Error: Failed to create security task\n");
        return -1;
    }
    printf("Security task created\n");
    

    /* System Monitor Task */
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

    printf("Starting scheduler...\n");
    xEventGroupSetBits(xSystemEvents, EVENT_DATA_READY);
    vTaskStartScheduler();
    printf("Error: Scheduler returned!\n");
    return -1;
}

void vDataProcessorTask(void *pvParameters) {
    sensor_data_t sensor_data;
    message_t network_msg;
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
            
            network_msg.data = sensor_data;
            network_msg.priority = (sensor_data.type == SENSOR_TYPE_MOTION) ? 2 : 1;
            network_msg.encrypted = false;

            if (xQueueSend(xNetworkQueue, &network_msg, pdMS_TO_TICKS(100)) != pdPASS) {
                safe_printf("Failed to send to network queue\n");
            }
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

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, 
                                  StackType_t **ppxIdleTaskStackBuffer,
                                  StackType_t *pulIdleTaskStackSize) {
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
    
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, 
                                   StackType_t **ppxTimerTaskStackBuffer,
                                   StackType_t *pulTimerTaskStackSize) {
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
    
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    printf("Stack overflow in task %s!\n", pcTaskName);
    configASSERT(0);
}