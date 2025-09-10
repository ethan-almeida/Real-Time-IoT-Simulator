#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#define EVENT_NETWORK_CONNECTED     (1 << 0)
#define EVENT_TLS_READY            (1 << 1)
#define EVENT_MQTT_CONNECTED       (1 << 2)
#define EVENT_DATA_READY           (1 << 3)
#define EVENT_SHUTDOWN             (1 << 4)

typedef enum {
    SENSOR_TYPE_TEMPERATURE,
    SENSOR_TYPE_HUMIDITY,
    SENSOR_TYPE_MOTION
} sensor_type_t;

typedef struct {
    sensor_type_t type;
    uint8_t sensor_id;
    float value;
    uint32_t timestamp;
} sensor_data_t;

typedef struct {
    sensor_data_t data;
    bool encrypted;
    uint8_t priority;
} message_t;

extern QueueHandle_t xSensorQueue;
extern QueueHandle_t xNetworkQueue;  
extern SemaphoreHandle_t xNetworkMutex;  
extern SemaphoreHandle_t xConsoleMutex;
extern EventGroupHandle_t xSystemEvents;
void safe_printf(const char *format, ...);
uint32_t get_system_time_ms(void);

#endif 