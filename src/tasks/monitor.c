#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "config.h"
#include "common.h"


#define MONITOR_UPDATE_INTERVAL_MS  2000
#define CONSOLE_WIDTH               80
#define MAX_TASK_NAME_LEN          16
#define HISTORY_SIZE               60
#define ANSI_RESET      "\033[0m"
#define ANSI_RED        "\033[31m"
#define ANSI_GREEN      "\033[32m"
#define ANSI_YELLOW     "\033[33m"
#define ANSI_BLUE       "\033[34m"
#define ANSI_MAGENTA    "\033[35m"
#define ANSI_CYAN       "\033[36m"
#define ANSI_BOLD       "\033[1m"
#define ANSI_CLEAR      "\033[2J\033[H"

typedef struct {
    uint32_t total_messages_processed;
    uint32_t messages_dropped;
    uint32_t network_packets_sent;
    uint32_t network_packets_failed;
    uint32_t uptime_seconds;
    float cpu_usage_percent;
    size_t heap_used;
    size_t heap_free;
    size_t heap_min_free;
} system_metrics_t;

typedef struct {
    char name[MAX_TASK_NAME_LEN];
    UBaseType_t priority;
    uint32_t stack_high_water;
    eTaskState state;
    uint32_t runtime_percent;
} task_info_t;

typedef struct {
    UBaseType_t sensor_queue_used;
    UBaseType_t sensor_queue_max;
    UBaseType_t network_queue_used;
    UBaseType_t network_queue_max;
    EventBits_t system_events;
} resource_monitor_t;

typedef struct {
    float values[HISTORY_SIZE];
    uint8_t index;
    uint8_t count;
} history_buffer_t;

static system_metrics_t metrics;
static history_buffer_t cpu_history;
static history_buffer_t memory_history;


static void draw_line(char ch, int width) {
    for (int i = 0; i < width; i++) {
        putchar(ch);
    }
    putchar('\n');
}

static void draw_progress_bar(const char *label, float value, float max_value, int width) {
    printf("%-15s [", label);
    
    int filled = (int)((value / max_value) * width);
    if (filled > width) filled = width;
    if (filled < 0) filled = 0;
    
    const char *color = ANSI_GREEN;
    if (value / max_value > 0.8) color = ANSI_RED;
    else if (value / max_value > 0.6) color = ANSI_YELLOW;
    
    printf("%s", color);
    for (int i = 0; i < filled; i++) {
        putchar('#');
    }
    printf("%s", ANSI_RESET);
    
    for (int i = filled; i < width; i++) {
        putchar('-');
    }
    
    printf("] %.1f%%\n", (value / max_value) * 100);
}

static void update_history(history_buffer_t *history, float value) {
    history->values[history->index] = value;
    history->index = (history->index + 1) % HISTORY_SIZE;
    if (history->count < HISTORY_SIZE) {
        history->count++;
    }
}

static void draw_graph(const char *label, history_buffer_t *history, float max_value) {
    printf("\n%s (Last %d samples, max: %.1f):\n", label, history->count, max_value);
    
    const int graph_height = 10;
    const int graph_width = (history->count < 60) ? history->count : 60;
    
    for (int row = graph_height; row >= 0; row--) {
        printf("%3.0f%% |", (float)row * 10);
        
        for (int col = 0; col < graph_width; col++) {
            int idx = (history->index - graph_width + col + HISTORY_SIZE) % HISTORY_SIZE;
            float normalized = (history->values[idx] / max_value) * 10;
            
            if (normalized >= row) {
                if (row > 8) printf("%s*%s", ANSI_RED, ANSI_RESET);
                else if (row > 6) printf("%s*%s", ANSI_YELLOW, ANSI_RESET);
                else printf("%s*%s", ANSI_GREEN, ANSI_RESET);
            } else {
                printf(" ");
            }
        }
        printf("\n");
    }
    
    printf("     +");
    for (int i = 0; i < graph_width; i++) printf("-");
    printf("\n");
}

static void get_task_stats(task_info_t *tasks, size_t *task_count) {
    *task_count = 0;
}

static void display_dashboard(void) {
    task_info_t tasks[10];
    size_t task_count = 0;
    resource_monitor_t resources;
    
    printf("%s", ANSI_CLEAR);
    printf("%s%s=== IoT Gateway Monitor ===%s\n", ANSI_BOLD, ANSI_CYAN, ANSI_RESET);
    printf("Uptime: %lu seconds | FreeRTOS v%s\n", metrics.uptime_seconds, tskKERNEL_VERSION_NUMBER);
    draw_line('=', CONSOLE_WIDTH);
    printf("\n%s%sSystem Status:%s\n", ANSI_BOLD, ANSI_GREEN, ANSI_RESET);
    printf("  System:   %sRUNNING%s\n", ANSI_GREEN, ANSI_RESET);
    printf("\n%s%sResource Usage:%s\n", ANSI_BOLD, ANSI_BLUE, ANSI_RESET);
    
    metrics.heap_used = 0;
    metrics.heap_free = configTOTAL_HEAP_SIZE;
    metrics.heap_min_free = configTOTAL_HEAP_SIZE;
    
    draw_progress_bar("Heap Memory", metrics.heap_used, configTOTAL_HEAP_SIZE, 40);
    printf("  Used: %zu bytes | Free: %zu bytes | Min Free: %zu bytes\n",
           metrics.heap_used, metrics.heap_free, metrics.heap_min_free);
    
    resources.sensor_queue_used = uxQueueMessagesWaiting(xSensorQueue);
    resources.sensor_queue_max = uxQueueSpacesAvailable(xSensorQueue) + resources.sensor_queue_used;
    resources.network_queue_used = 0;
    resources.network_queue_max = 0;
    
    printf("\n");
    draw_progress_bar("Sensor Queue", resources.sensor_queue_used, resources.sensor_queue_max, 40);
    printf("\n%s%sTask Information:%s\n", ANSI_BOLD, ANSI_MAGENTA, ANSI_RESET);
    printf("%-16s | Priority | Stack | State | CPU%%\n", "Task Name");
    draw_line('-', 60);
    
    get_task_stats(tasks, &task_count);
    for (size_t i = 0; i < task_count; i++) {
        const char *state_str;
        const char *state_color;
        
        switch (tasks[i].state) {
            case eRunning:
                state_str = "RUN";
                state_color = ANSI_GREEN;
                break;
            case eReady:
                state_str = "RDY";
                state_color = ANSI_CYAN;
                break;
            case eBlocked:
                state_str = "BLK";
                state_color = ANSI_YELLOW;
                break;
            case eSuspended:
                state_str = "SUS";
                state_color = ANSI_RED;
                break;
            default:
                state_str = "DEL";
                state_color = ANSI_RED;
                break;
        }
        
        printf("%-16s | %8u | %5lu | %s%-3s%s | %3lu%%\n",
               tasks[i].name,
               tasks[i].priority,
               tasks[i].stack_high_water,
               state_color, state_str, ANSI_RESET,
               tasks[i].runtime_percent);
    }
    
    printf("\n%s%sPerformance Metrics:%s\n", ANSI_BOLD, ANSI_YELLOW, ANSI_RESET);
    printf("  Messages Processed:  %lu\n", metrics.total_messages_processed);
    printf("  Messages Dropped:    %lu\n", metrics.messages_dropped);
    update_history(&cpu_history, metrics.cpu_usage_percent);
    update_history(&memory_history, (float)metrics.heap_used / configTOTAL_HEAP_SIZE * 100);
    draw_graph("CPU Usage", &cpu_history, 100.0);
    draw_graph("Memory Usage", &memory_history, 100.0);
    draw_line('=', CONSOLE_WIDTH);
    printf("Press Ctrl+C to exit | Updates every %d seconds\n", MONITOR_UPDATE_INTERVAL_MS / 1000);
}


static float get_cpu_usage(void) {
    return 0.0f;
}

void vMonitorTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    safe_printf("[Monitor] Started - Dashboard updates every %d seconds\n",
                MONITOR_UPDATE_INTERVAL_MS / 1000);
    
    memset(&metrics, 0, sizeof(metrics));
    memset(&cpu_history, 0, sizeof(cpu_history));
    memset(&memory_history, 0, sizeof(memory_history));

    vTaskDelay(pdMS_TO_TICKS(3000));
    
    for (;;) {
        metrics.uptime_seconds = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
        metrics.cpu_usage_percent = get_cpu_usage();
        metrics.total_messages_processed = 0;  
        
        if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdTRUE) {
            display_dashboard();
            xSemaphoreGive(xConsoleMutex);
        }
        
        EventBits_t events = xEventGroupGetBits(xSystemEvents);
        if (events & EVENT_SHUTDOWN) {
            safe_printf("\n[Monitor] Shutting down\n");
            break;
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(MONITOR_UPDATE_INTERVAL_MS));
    }
    
    vTaskDelete(NULL);
}