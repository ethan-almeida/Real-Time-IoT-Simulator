#include <stdio.h>
#include <string.h>
#include <math.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "config.h"
#include "common.h"

#define AVERAGING_WINDOW_SIZE   5
#define ANOMALY_THRESHOLD       3.0f  
#define BATCH_SIZE             10
#define BATCH_TIMEOUT_MS       5000

typedef struct {
    float min_value;
    float max_value;
    float sum;
    float sum_squared;
    uint32_t count;
    float window[AVERAGING_WINDOW_SIZE];
    uint8_t window_index;
} sensor_stats_t;


static sensor_stats_t temp_stats[NUM_TEMP_SENSORS];
static sensor_stats_t humidity_stats[NUM_HUMIDITY_SENSORS];
static sensor_stats_t motion_stats;
static message_t batch_buffer[BATCH_SIZE];
static uint8_t batch_count = 0;
static TickType_t last_batch_time;

static BaseType_t send_to_network_queue(const message_t *msg, TickType_t timeout) {
    BaseType_t result = xQueueSend(xNetworkQueue, msg, timeout);
    
    if (result != pdPASS) {
        UBaseType_t messages_waiting = uxQueueMessagesWaiting(xNetworkQueue);
        if (messages_waiting >= NETWORK_QUEUE_SIZE) {
            safe_printf("[DataProcessor] Network queue full (%u messages)\n", messages_waiting);
            if (msg->priority >= 2) {
                message_t old_msg;
                if (xQueueReceive(xNetworkQueue, &old_msg, 0) == pdPASS) {
                    result = xQueueSend(xNetworkQueue, msg, 0);
                    if (result == pdPASS) {
                        safe_printf("[DataProcessor] Dropped old message for high priority one\n");
                    }
                }
            }
        }
    }
    
    return result;
}


static void init_sensor_stats(sensor_stats_t *stats) {
    stats->min_value = INFINITY;
    stats->max_value = -INFINITY;
    stats->sum = 0.0f;
    stats->sum_squared = 0.0f;
    stats->count = 0;
    stats->window_index = 0;
    memset(stats->window, 0, sizeof(stats->window));
}

static void update_statistics(sensor_stats_t *stats, float value) {
    if (value < stats->min_value) {
        stats->min_value = value;
    }

    if (value > stats->max_value) {
        stats->max_value = value;
    }

    stats->sum += value;
    stats->sum_squared += value * value;
    stats->count++;
    stats->window[stats->window_index] = value;
    stats->window_index = (stats->window_index + 1) % AVERAGING_WINDOW_SIZE;
}

static float calculate_moving_average(const sensor_stats_t *stats) {
    float sum = 0.0f;
    uint8_t count = (stats->count < AVERAGING_WINDOW_SIZE) ? stats->count : AVERAGING_WINDOW_SIZE;
    
    for (uint8_t i = 0; i < count; i++) {
        sum += stats->window[i];
    }
    
    return (count > 0) ? (sum / count) : 0.0f;
}

static bool is_anomaly(const sensor_stats_t *stats, float value) {
    if (stats->count < AVERAGING_WINDOW_SIZE) {
        return false;  
    }
    
    float mean = stats->sum / stats->count;
    float variance = (stats->sum_squared / stats->count) - (mean * mean);
    float std_dev = sqrtf(variance);
    
    if (std_dev < 0.001f) {
        return false;  
    }
    
    float z_score = fabsf(value - mean) / std_dev;
    return z_score > ANOMALY_THRESHOLD;
}

static void process_sensor_data(const sensor_data_t *data) {
    sensor_stats_t *stats = NULL;
    const char *sensor_name = NULL;
    bool anomaly_detected = false;

    switch (data->type) {
        case SENSOR_TYPE_TEMPERATURE:
            if (data->sensor_id < NUM_TEMP_SENSORS) {
                stats = &temp_stats[data->sensor_id];
                sensor_name = "Temperature";
            }
            break;
            
        case SENSOR_TYPE_HUMIDITY:
            if (data->sensor_id < NUM_HUMIDITY_SENSORS) {
                stats = &humidity_stats[data->sensor_id];
                sensor_name = "Humidity";
            }
            break;
            
        case SENSOR_TYPE_MOTION:
            stats = &motion_stats;
            sensor_name = "Motion";
            break;
    }
    
    if (stats == NULL) {
        safe_printf("[DataProcessor] Invalid sensor data received\n");
        return;
    }
    
    anomaly_detected = is_anomaly(stats, data->value);
    update_statistics(stats, data->value);
    float avg_value = calculate_moving_average(stats);
    safe_printf("[DataProcessor] %s sensor %d: %.2f (avg: %.2f)%s\n",
                sensor_name, data->sensor_id, data->value, avg_value,
                anomaly_detected ? " ANOMALY!" : "");

    if ((data->type == SENSOR_TYPE_MOTION && data->value > 0.5f) || anomaly_detected) {
        message_t immediate_msg;
        immediate_msg.data = *data;
        immediate_msg.encrypted = false;
        immediate_msg.priority = (data->type == SENSOR_TYPE_MOTION) ? 3 : 2;
        
        if (xQueueSend(xNetworkQueue, &immediate_msg, pdMS_TO_TICKS(100)) != pdPASS) {
            safe_printf("[DataProcessor] Failed to send high-priority message\n");
        } else {
            safe_printf("[DataProcessor] Sent immediate %s message\n", 
                       anomaly_detected ? "anomaly" : "motion");
        }
        return; 
    }

    if (batch_count < BATCH_SIZE) {
        message_t *msg = &batch_buffer[batch_count];
        msg->data = *data;
        msg->encrypted = false;
        msg->priority = 1;
        batch_count++;
    } else {
        safe_printf("[DataProcessor] Batch buffer full, dropping message\n");
    }
}

void vDataProcessorTask(void *pvParameters) {
    sensor_data_t sensor_data;
    safe_printf("[DataProcessor] Started\n");
    
    for (int i = 0; i < NUM_TEMP_SENSORS; i++) {
        init_sensor_stats(&temp_stats[i]);
    }
    for (int i = 0; i < NUM_HUMIDITY_SENSORS; i++) {
        init_sensor_stats(&humidity_stats[i]);
    }
    init_sensor_stats(&motion_stats);
    
    last_batch_time = xTaskGetTickCount();
    batch_count = 0;
    
    safe_printf("[DataProcessor] Waiting for network connection...\n");
    xEventGroupWaitBits(xSystemEvents, EVENT_MQTT_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);
    safe_printf("[DataProcessor] Network connected, starting processing\n");
    
    for (;;) {
        if (xQueueReceive(xSensorQueue, &sensor_data, pdMS_TO_TICKS(100)) == pdPASS) {
            if (xSemaphoreTake(g_latest_readings.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                switch (sensor_data.type) {
                    case SENSOR_TYPE_TEMPERATURE:
                        if (sensor_data.sensor_id < 3) {
                            g_latest_readings.temperature[sensor_data.sensor_id] = sensor_data.value;
                        }
                        break;
                    case SENSOR_TYPE_HUMIDITY:
                        if (sensor_data.sensor_id < 2) {
                            g_latest_readings.humidity[sensor_data.sensor_id] = sensor_data.value;
                        }
                        break;
                    case SENSOR_TYPE_MOTION:
                        g_latest_readings.motion = sensor_data.value;
                        break;
                }
                g_latest_readings.last_update = get_system_time_ms();
                xSemaphoreGive(g_latest_readings.mutex);
            }
            process_sensor_data(&sensor_data);
        }

        if (batch_count > 0 && 
            (xTaskGetTickCount() - last_batch_time) > pdMS_TO_TICKS(BATCH_TIMEOUT_MS)) {
            for (int i = 0; i < batch_count; i++) {
                if (xQueueSend(xNetworkQueue, &batch_buffer[i], pdMS_TO_TICKS(50)) != pdPASS) {
                    safe_printf("[DataProcessor] Failed to send message %d/%d to network queue\n", 
                               i+1, batch_count);
                }
            }
            safe_printf("[DataProcessor] Flushed batch of %d messages\n", batch_count);
            batch_count = 0;
            last_batch_time = xTaskGetTickCount();
        }
    }
}