/*
 * Copyright (c) 2017 Simon Goldschmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Simon Goldschmidt
 *
 */

#include "sys_arch.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

/* Handle to the task that is running the network thread */
static TaskHandle_t network_thread_handle;

/* Current time in milliseconds */
static u32_t current_time_ms;

/* Initialize the system */
void sys_init(void)
{
    current_time_ms = 0;
}

/* Create a new semaphore */
err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    *sem = xQueueCreateCountingSemaphore(0xFFFF, count);
    if (*sem == NULL) {
        return ERR_MEM;
    }
    return ERR_OK;
}

/* Delete a semaphore */
void sys_sem_free(sys_sem_t *sem)
{
    if (*sem != NULL) {
        vSemaphoreDelete(*sem);
        *sem = NULL;
    }
}

/* Signal a semaphore */
void sys_sem_signal(sys_sem_t *sem)
{
    if (*sem != NULL) {
        xSemaphoreGive(*sem);
    }
}

/* Wait for a semaphore for the specified timeout */
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    if (*sem == NULL) {
        return SYS_ARCH_TIMEOUT;
    }

    if (timeout == 0) {
        /* Wait forever */
        while (xSemaphoreTake(*sem, portMAX_DELAY) != pdTRUE) {
            /* Should not happen with portMAX_DELAY */
        }
        return 0;
    } else {
        /* Wait for specified time */
        TickType_t ticks = timeout / portTICK_PERIOD_MS;
        if (ticks == 0) {
            ticks = 1;
        }
        
        if (xSemaphoreTake(*sem, ticks) == pdTRUE) {
            return timeout; /* Just return a non-zero value */
        } else {
            return SYS_ARCH_TIMEOUT;
        }
    }
}

/* Check if a semaphore is valid */
int sys_sem_valid(sys_sem_t *sem)
{
    return (*sem != NULL) ? 1 : 0;
}

/* Set a semaphore invalid */
void sys_sem_set_invalid(sys_sem_t *sem)
{
    *sem = NULL;
}

/* Create a new mutex */
err_t sys_mutex_new(sys_mutex_t *mutex)
{
    *mutex = xSemaphoreCreateMutex();
    if (*mutex == NULL) {
        return ERR_MEM;
    }
    return ERR_OK;
}

/* Delete a mutex */
void sys_mutex_free(sys_mutex_t *mutex)
{
    if (*mutex != NULL) {
        vSemaphoreDelete(*mutex);
        *mutex = NULL;
    }
}

/* Lock a mutex */
void sys_mutex_lock(sys_mutex_t *mutex)
{
    if (*mutex != NULL) {
        xSemaphoreTake(*mutex, portMAX_DELAY);
    }
}

/* Unlock a mutex */
void sys_mutex_unlock(sys_mutex_t *mutex)
{
    if (*mutex != NULL) {
        xSemaphoreGive(*mutex);
    }
}

/* Check if a mutex is valid */
int sys_mutex_valid(sys_mutex_t *mutex)
{
    return (*mutex != NULL) ? 1 : 0;
}

/* Set a mutex invalid */
void sys_mutex_set_invalid(sys_mutex_t *mutex)
{
    *mutex = NULL;
}

/* Create a new mbox */
err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    *mbox = xQueueCreate(size, sizeof(void*));
    if (*mbox == NULL) {
        return ERR_MEM;
    }
    return ERR_OK;
}

/* Delete an mbox */
void sys_mbox_free(sys_mbox_t *mbox)
{
    if (*mbox != NULL) {
        vQueueDelete(*mbox);
        *mbox = NULL;
    }
}

/* Post a message to an mbox - may not fail -> task context */
void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    if (*mbox != NULL) {
        while (xQueueSendToBack(*mbox, &msg, portMAX_DELAY) != pdTRUE) {
            /* Should not happen with portMAX_DELAY */
        }
    }
}

/* Try to post a message to an mbox - may fail if full */
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    if (*mbox == NULL) {
        return ERR_ARG;
    }

    if (xQueueSendToBack(*mbox, &msg, 0) == pdTRUE) {
        return ERR_OK;
    } else {
        return ERR_MEM;
    }
}

/* Try to post a message to an mbox - may fail if full */
err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
    BaseType_t ret, xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;

    if (*mbox == NULL) {
        return ERR_ARG;
    }

    ret = xQueueSendToBackFromISR(*mbox, &msg, &xHigherPriorityTaskWoken);
    if (ret == pdTRUE) {
        return ERR_OK;
    } else {
        return ERR_MEM;
    }
}

/* Fetch a message from an mbox */
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    void *msg_ptr;
    
    if (*mbox == NULL) {
        return SYS_ARCH_TIMEOUT;
    }

    if (timeout == 0) {
        /* Wait forever */
        while (xQueueReceive(*mbox, &msg_ptr, portMAX_DELAY) != pdTRUE) {
            /* Should not happen with portMAX_DELAY */
        }
        if (msg != NULL) {
            *msg = msg_ptr;
        }
        return 0;
    } else {
        /* Wait for specified time */
        TickType_t ticks = timeout / portTICK_PERIOD_MS;
        if (ticks == 0) {
            ticks = 1;
        }
        
        if (xQueueReceive(*mbox, &msg_ptr, ticks) == pdTRUE) {
            if (msg != NULL) {
                *msg = msg_ptr;
            }
            return timeout; /* Just return a non-zero value */
        } else {
            if (msg != NULL) {
                *msg = NULL;
            }
            return SYS_ARCH_TIMEOUT;
        }
    }
}

/* Try to fetch a message from an mbox */
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    void *msg_ptr;
    
    if (*mbox == NULL) {
        return SYS_ARCH_TIMEOUT;
    }

    if (xQueueReceive(*mbox, &msg_ptr, 0) == pdTRUE) {
        if (msg != NULL) {
            *msg = msg_ptr;
        }
        return 0;
    } else {
        if (msg != NULL) {
            *msg = NULL;
        }
        return SYS_ARCH_TIMEOUT;
    }
}

/* Check if an mbox is valid */
int sys_mbox_valid(sys_mbox_t *mbox)
{
    return (*mbox != NULL) ? 1 : 0;
}

/* Set an mbox invalid */
void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
    *mbox = NULL;
}

/* Create a new thread */
sys_thread_t sys_thread_new(const char *name, void (*thread)(void *), void *arg, int stacksize, int prio)
{
    TaskHandle_t task_handle;
    
    if (xTaskCreate(thread, name, stacksize, arg, prio, &task_handle) != pdPASS) {
        return NULL;
    }
    
    if (strcmp(name, "network_thread") == 0) {
        network_thread_handle = task_handle;
    }
    
    return task_handle;
}

/* Get the current time in milliseconds */
u32_t sys_now(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/* Sleep for specified number of milliseconds */
void sys_msleep(u32_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}