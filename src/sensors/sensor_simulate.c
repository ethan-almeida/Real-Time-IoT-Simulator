#include "FreeRTOS.h"
#include "task.h"
#include "common.h"

void vSensorTask(void *pvParameters) {
    // Placeholder sensor task
    for(;;) {
        // Sensor logic here
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}