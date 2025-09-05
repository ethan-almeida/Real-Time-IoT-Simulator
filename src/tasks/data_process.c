#include <stdio.h>
#include <string.h>
#include <math.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "config.h"
#include "common.h"

/* Data processing configuration */
#define AVERAGING_WINDOW_SIZE   5
#define ANOMALY_THRESHOLD       3.0f  /* Standard deviations */
#define BATCH_SIZE             10
#define BATCH_TIMEOUT_MS       5000

/* Sensor statistics structure */
typedef struct {
    float min_value;
    float max_value;
    float sum;
    float sum_squared;
    uint32_t count;
    float window[AVERAGING_WINDOW_SIZE];
    uint8_t window_index;
} sensor_stats_t;

/* Global statistics for each sensor type */
static sensor_stats_t temp_stats[NUM_TEMP_SENSORS];
static sensor_stats_t humidity_stats[NUM_HUMIDITY_SENSORS];
static sensor_stats_t motion_stats;

/* Batch buffer for aggregated data */
static message_t batch_buffer[BATCH_SIZE];
static uint8_t batch_count = 0;
static TickType_t last_batch_time;

/* Initialize sensor statistics */
static void init_sensor_stats(sensor_stats_t *stats) {
    stats->min_value = INFINITY;
    stats->max_value = -INFINITY;
    stats->sum = 0.0f;
    stats->sum_squared = 0.0f;
    stats->count = 0;
    stats->window_index = 0;
    memset(stats->window, 0, sizeof(stats->window));
}

/* Update statistics with new sensor reading */
static void update_statistics(sensor_stats_t *stats, float value) {
    /* Update min/max */
    if (value < stats->min_value) stats->min_value = value;
    if (value > stats->max_value) stats->max_value = value;
    
    /* Update running sums */
    stats->sum += value;
    stats->sum_squared += value * value;
    stats->count++;
    
    /* Update moving average window */
    stats->window[stats->window_index] = value;
    stats->window_index = (stats->window_index + 1) % AVERAGING_WINDOW_SIZE;
}

/* Calculate moving average */
static float calculate_moving_average(const sensor_stats_t *stats) {
    float sum = 0.0f;
    uint8_t count = (stats->count < AVERAGING_WINDOW_SIZE) ? stats->count : AVERAGING_WINDOW_SIZE;
    
    for (uint8_t i = 0; i < count; i++) {
        sum += stats->window[i];
    }
    
    return (count > 0) ? (sum / count) : 0.0f;
}

/* Check for anomalies using z-score */
static bool is_anomaly(const sensor_stats_t *stats, float value) {
    if (stats->count < AVERAGING_WINDOW_SIZE) {
        return false;  /* Not enough data yet */
    }
    
    float mean = stats->sum / stats->count;
    float variance = (stats->sum_squared / stats->count) - (mean * mean);
    float std_dev = sqrtf(variance);
    
    if (std_dev < 0.001f) {
        return false;  /* No variation in data */
    }
    
    float z_score = fabsf(value - mean) / std_dev;
    return z_score > ANOMALY_THRESHOLD;
}

/* Process sensor data and prepare for transmission */
static void process_sensor_data(const sensor_data_t *data) {
    sensor_stats_t *stats = NULL;
    const char *sensor_name = NULL;
    bool anomaly_detected = false;
    
    /* Select appropriate statistics structure */
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
    
    /* Check for anomalies before updating stats */
    anomaly_detected = is_anomaly(stats, data->value);
    
    /* Update statistics */
    update_statistics(stats, data->value);
    
    /* Calculate moving average */
    float avg_value = calculate_moving_average(stats);
    
    /* Log processed data */
    safe_printf("[DataProcessor] %s[%d]: %.2f (avg: %.2f)%s\n",
                sensor_name, data->sensor_id, data->value, avg_value,
                anomaly_detected ? " ANOMALY!" : "");
    
    /* Prepare message for network transmission */
    message_t *msg = &batch_buffer[batch_count];
    msg->data = *data;
    msg->encrypted = false;
    msg->priority = anomaly_detected ? 2 : 1;
    
    /* For motion sensors, send immediately */
    if (data->type == SENSOR_TYPE_MOTION && data->value > 0.5f) {
        msg->priority = 3;  /* High priority for motion detection */
        // Disabled network queue for now
        safe_printf("[DataProcessor] Motion event detected\n");
        return;
    }
    
    /* Add to batch */
    batch_count++;
    
    /* Send batch if full or timeout */
    if (batch_count >= BATCH_SIZE || 
        (xTaskGetTickCount() - last_batch_time) > pdMS_TO_TICKS(BATCH_TIMEOUT_MS)) {
        
        /* Process all messages in batch */
        safe_printf("[DataProcessor] Processed batch of %d messages\n", batch_count);
        batch_count = 0;
        last_batch_time = xTaskGetTickCount();
    }
}

void vDataProcessorTask(void *pvParameters) {
    sensor_data_t sensor_data;
    
    safe_printf("[DataProcessor] Started\n");
    
    /* Initialize all statistics */
    for (int i = 0; i < NUM_TEMP_SENSORS; i++) {
        init_sensor_stats(&temp_stats[i]);
    }
    for (int i = 0; i < NUM_HUMIDITY_SENSORS; i++) {
        init_sensor_stats(&humidity_stats[i]);
    }
    init_sensor_stats(&motion_stats);
    
    last_batch_time = xTaskGetTickCount();
    
    /* Signal that data processor is ready */
    xEventGroupSetBits(xSystemEvents, EVENT_DATA_READY);
    
    for (;;) {
        /* Wait for sensor data with timeout for batch processing */
        if (xQueueReceive(xSensorQueue, &sensor_data, pdMS_TO_TICKS(1000)) == pdPASS) {
            process_sensor_data(&sensor_data);
        } else {
            /* Timeout - check if we need to flush batch */
            if (batch_count > 0 && 
                (xTaskGetTickCount() - last_batch_time) > pdMS_TO_TICKS(BATCH_TIMEOUT_MS)) {
                
                /* Process partial batch */
                safe_printf("[DataProcessor] Processed partial batch of %d messages\n", batch_count);
                batch_count = 0;
                last_batch_time = xTaskGetTickCount();
            }
        }
        
        /* Check for shutdown event */
        EventBits_t events = xEventGroupGetBits(xSystemEvents);
        if (events & EVENT_SHUTDOWN) {
            safe_printf("[DataProcessor] Shutting down\n");
            break;
        }
    }
    
    vTaskDelete(NULL);
}