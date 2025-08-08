#define configUSE_PREEMPTION        1
#define configUSE_IDLE_HOOK         0
#define configUSE_TICK_HOOK         0
#define configCPU_CLOCK_HZ         ( 48000000UL ) // 48 MHz
#define configTICK_RATE_HZ         ( 1000 )       // 1 kHz
#define configMAX_PRIORITIES       ( 5 )
#define configMINIMAL_STACK_SIZE   ( 128 )
#define configTOTAL_HEAP_SIZE      ( ( size_t ) ( 10 * 1024 ) ) // 10 KB
#define configMAX_TASK_NAME_LEN    ( 16 )
#define configUSE_TRACE_FACILITY   0
#define configUSE_16_BIT_TICKS     0
#define configIDLE_SHOULD_YIELD    1
#define configUSE_MUTEXES           1
#define configQUEUE_REGISTRY_SIZE   10
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_MALLOC_FAILED_HOOK 1
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_APPLICATION_TASK_TAG 0
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_TIMERS            1
#define configTIMER_TASK_PRIORITY   ( 2 )
#define configTIMER_QUEUE_LENGTH    10
#define configTIMER_TASK_STACK_DEPTH ( configMINIMAL_STACK_SIZE * 2 )