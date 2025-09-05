#ifndef TASK_PRIORITIES_H
#define TASK_PRIORITIES_H

/* Task Priorities (higher number = higher priority) */
#define PRIORITY_MONITOR        (tskIDLE_PRIORITY + 1)
#define PRIORITY_SENSOR_LOW     (tskIDLE_PRIORITY + 2)
#define PRIORITY_SENSOR_HIGH    (tskIDLE_PRIORITY + 3)
#define PRIORITY_PROCESSOR      (tskIDLE_PRIORITY + 4)
#define PRIORITY_NETWORK        (tskIDLE_PRIORITY + 5)
#define PRIORITY_SECURITY       (tskIDLE_PRIORITY + 6)

#endif /* TASK_PRIORITIES_H */