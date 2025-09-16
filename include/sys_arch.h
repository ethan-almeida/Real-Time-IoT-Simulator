#ifndef SYS_ARCH_H
#define SYS_ARCH_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#define ERR_OK          0    
#define ERR_MEM        -1    
#define ERR_ARG        -2  
#define SYS_ARCH_TIMEOUT 0xffffffffU  

typedef int err_t;
typedef uint8_t u8_t;
typedef uint32_t u32_t;
typedef xSemaphoreHandle sys_sem_t;
typedef xSemaphoreHandle sys_mutex_t;
typedef xQueueHandle sys_mbox_t;
typedef TaskHandle_t sys_thread_t;

void sys_init(void);
err_t sys_sem_new(sys_sem_t *sem, u8_t count);
void sys_sem_free(sys_sem_t *sem);
void sys_sem_signal(sys_sem_t *sem);
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout);
int sys_sem_valid(sys_sem_t *sem);
void sys_sem_set_invalid(sys_sem_t *sem);
err_t sys_mutex_new(sys_mutex_t *mutex);
void sys_mutex_free(sys_mutex_t *mutex);
void sys_mutex_lock(sys_mutex_t *mutex);
void sys_mutex_unlock(sys_mutex_t *mutex);
int sys_mutex_valid(sys_mutex_t *mutex);
void sys_mutex_set_invalid(sys_mutex_t *mutex);
err_t sys_mbox_new(sys_mbox_t *mbox, int size);
void sys_mbox_free(sys_mbox_t *mbox);
void sys_mbox_post(sys_mbox_t *mbox, void *msg);
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg);
err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg);
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout);
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg);
int sys_mbox_valid(sys_mbox_t *mbox);
void sys_mbox_set_invalid(sys_mbox_t *mbox);
sys_thread_t sys_thread_new(const char *name, void (*thread)(void *), void *arg, int stacksize, int prio);
u32_t sys_now(void);
void sys_msleep(u32_t ms);

#endif 