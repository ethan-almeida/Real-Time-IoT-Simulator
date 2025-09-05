#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS types for lwIP */
typedef SemaphoreHandle_t sys_sem_t;
typedef SemaphoreHandle_t sys_mutex_t;
typedef QueueHandle_t sys_mbox_t;
typedef TaskHandle_t sys_thread_t;

/* Message structure for mailboxes */
struct sys_mbox_msg {
    void *msg;
};

/* Timeout value */
#define SYS_ARCH_TIMEOUT 0xffffffffUL

/* Thread priority for lwIP threads */
#define TCPIP_THREAD_PRIO 3

#endif /* LWIP_ARCH_SYS_ARCH_H */