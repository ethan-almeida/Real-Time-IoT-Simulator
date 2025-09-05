#include <stdlib.h>
#include <math.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "config.h"
#include "common.h"

/* Sensor simulation parameters */
#define TEMP_BASE           20.0f
#define TEMP_VARIATION      5.0f
#define HUMIDITY_BASE       50.0f
#define HUMIDITY_VARIATION  20.0f
#define MOTION_THRESHOLD    0.7f

/* Simulate temperature reading with some noise */
static float simulate_temperature(uint8_t sensor_id) {
    float base = TEMP_BASE + (sensor_id * 2.0f);
    float noise = ((float)rand() / RAND_MAX - 0.5f) * TEMP_VARIATION;
    float seasonal = sinf(get_system_time_ms() / 60000.0f) * 3.0f;
    return base + noise + seasonal;
}

/* Simulate humidity reading */
static float simulate_humidity(uint8_t sensor_id) {
    float base = HUMIDITY_BASE + (sensor_id * 5.0f);
    float noise = ((float)rand() / RAND_MAX - 0.5f) * HUMIDITY_VARIATION;
    return base + noise;
}

/* Simulate motion detection */
static bool simulate_motion(void) {
    return ((float)rand() / RAND_MAX) > MOTION_THRESHOLD;
}

void vTemperatureSensorTask(void *pvParameters) {
    uint8_t sensor_id = (uint8_t)(intptr_t)pvParameters;
    sensor_data_t sensor_data;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    safe_printf("[TempSensor%d] Started\n", sensor_id);
    
    for (;;) {
        /* Simulate temperature reading */
        sensor_data.type = SENSOR_TYPE_TEMPERATURE;
        sensor_data.sensor_id = sensor_id;
        sensor_data.value = simulate_temperature(sensor_id);
        sensor_data.timestamp = get_system_time_ms();
        
        /* Send to queue */
        if (xQueueSend(xSensorQueue, &sensor_data, pdMS_TO_TICKS(100)) != pdPASS) {
            safe_printf("[TempSensor%d] Queue full, dropping reading\n", sensor_id);
        }
        
        /* Wait for next reading interval */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}

void vHumiditySensorTask(void *pvParameters) {
    uint8_t sensor_id = (uint8_t)(intptr_t)pvParameters;
    sensor_data_t sensor_data;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    safe_printf("[HumidSensor%d] Started\n", sensor_id);
    
    for (;;) {
        /* Simulate humidity reading */
        sensor_data.type = SENSOR_TYPE_HUMIDITY;
        sensor_data.sensor_id = sensor_id;
        sensor_data.value = simulate_humidity(sensor_id);
        sensor_data.timestamp = get_system_time_ms();
        
        /* Send to queue */
        if (xQueueSend(xSensorQueue, &sensor_data, pdMS_TO_TICKS(100)) != pdPASS) {
            safe_printf("[HumidSensor%d] Queue full, dropping reading\n", sensor_id);
        }
        
        /* Wait for next reading interval */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS * 2));
    }
}

void vMotionSensorTask(void *pvParameters) {
    sensor_data_t sensor_data;
    bool last_motion = false;
    
    safe_printf("[MotionSensor] Started\n");
    
    for (;;) {
        /* Simulate motion detection */
        bool motion_detected = simulate_motion();
        
        /* Only send on state change */
        if (motion_detected != last_motion) {
            sensor_data.type = SENSOR_TYPE_MOTION;
            sensor_data.sensor_id = 0;
            sensor_data.value = motion_detected ? 1.0f : 0.0f;
            sensor_data.timestamp = get_system_time_ms();
            
            /* Motion events have higher priority */
            if (xQueueSendToFront(xSensorQueue, &sensor_data, pdMS_TO_TICKS(100)) == pdPASS) {
                safe_printf("[MotionSensor] Motion %s\n", 
                           motion_detected ? "DETECTED" : "CLEARED");
            } else {
                safe_printf("[MotionSensor] Failed to send motion event\n");
            }
            
            last_motion = motion_detected;
        }
        
        /* Check for motion every 500ms */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}