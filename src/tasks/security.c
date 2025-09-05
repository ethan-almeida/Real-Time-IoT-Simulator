#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "config.h"
#include "common.h"

/* Simplified Security Configuration */
#define AES_KEY_SIZE            32
#define AES_BLOCK_SIZE          16
#define MAX_ENCRYPTED_SIZE      512
#define KEY_ROTATION_INTERVAL   (60 * 60 * 1000)  /* 1 hour in ms */

/* Security statistics */
typedef struct {
    uint32_t messages_encrypted;
    uint32_t messages_signed;
    uint32_t key_rotations;
    uint32_t security_errors;
} security_stats_t;

/* Simplified security context */
typedef struct {
    uint8_t aes_key[AES_KEY_SIZE];
    uint8_t session_key[32];
    TickType_t last_key_rotation;
    security_stats_t stats;
    bool initialized;
} security_context_t;

static security_context_t sec_ctx;

/* Simple XOR-based encryption for simulation */
static void simple_encrypt(const uint8_t *input, uint8_t *output, size_t len, const uint8_t *key) {
    for (size_t i = 0; i < len; i++) {
        output[i] = input[i] ^ key[i % AES_KEY_SIZE];
    }
}

/* Simple hash function for simulation */
static uint32_t simple_hash(const uint8_t *data, size_t len) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + data[i];
    }
    return hash;
}

/* Initialize security context */
static int init_security_context(void) {
    safe_printf("[Security] Initializing simplified security context...\n");
    
    /* Generate simple keys using system time and random */
    srand(time(NULL));
    
    for (int i = 0; i < AES_KEY_SIZE; i++) {
        sec_ctx.aes_key[i] = rand() & 0xFF;
    }
    
    for (int i = 0; i < 32; i++) {
        sec_ctx.session_key[i] = rand() & 0xFF;
    }
    
    /* Initialize statistics */
    memset(&sec_ctx.stats, 0, sizeof(sec_ctx.stats));
    sec_ctx.last_key_rotation = xTaskGetTickCount();
    sec_ctx.initialized = true;
    
    safe_printf("[Security] Simplified security context initialized\n");
    return 0;
}

/* Rotate encryption keys */
static int rotate_keys(void) {
    safe_printf("[Security] Rotating encryption keys...\n");
    
    /* Generate new keys */
    for (int i = 0; i < AES_KEY_SIZE; i++) {
        sec_ctx.aes_key[i] = rand() & 0xFF;
    }
    
    for (int i = 0; i < 32; i++) {
        sec_ctx.session_key[i] = rand() & 0xFF;
    }
    
    sec_ctx.stats.key_rotations++;
    sec_ctx.last_key_rotation = xTaskGetTickCount();
    
    safe_printf("[Security] Key rotation completed (rotation #%u)\n", 
                (unsigned int)sec_ctx.stats.key_rotations);
    return 0;
}

/* Encrypt data using simple XOR */
static int encrypt_data(const uint8_t *plaintext, size_t plaintext_len,
                       uint8_t *ciphertext, size_t *ciphertext_len) {
    
    if (plaintext_len > MAX_ENCRYPTED_SIZE) {
        safe_printf("[Security] Data too large to encrypt\n");
        sec_ctx.stats.security_errors++;
        return -1;
    }
    
    /* Simple XOR encryption */
    simple_encrypt(plaintext, ciphertext, plaintext_len, sec_ctx.aes_key);
    *ciphertext_len = plaintext_len;
    
    sec_ctx.stats.messages_encrypted++;
    return 0;
}

/* Create simple signature */
static int sign_data(const uint8_t *data, size_t data_len, uint32_t *signature) {
    /* Create a simple hash-based signature */
    *signature = simple_hash(data, data_len);
    
    /* XOR with session key for "signing" */
    uint32_t key_hash = simple_hash(sec_ctx.session_key, 32);
    *signature ^= key_hash;
    
    sec_ctx.stats.messages_signed++;
    return 0;
}

void vSecurityTask(void *pvParameters) {
    (void)pvParameters;  /* Suppress unused parameter warning */
    
    message_t msg;
    uint8_t encrypted_buffer[MAX_ENCRYPTED_SIZE];
    size_t encrypted_len;
    uint32_t signature;
    char status_msg[128];
    
    safe_printf("[Security] Started (Simplified Mode)\n");
    
    /* Initialize security context */
    if (init_security_context() != 0) {
        safe_printf("[Security] Failed to initialize, task terminating\n");
        vTaskDelete(NULL);
        return;
    }
    
    for (;;) {
        /* Check if key rotation is needed */
        if ((xTaskGetTickCount() - sec_ctx.last_key_rotation) > 
            pdMS_TO_TICKS(KEY_ROTATION_INTERVAL)) {
            rotate_keys();
        }
        
        /* Process security operations from network queue */
        if (uxQueueMessagesWaiting(xNetworkQueue) > 0) {
            /* Peek at message to check if it needs encryption */
            if (xQueuePeek(xNetworkQueue, &msg, 0) == pdPASS) {
                if (!msg.encrypted && msg.priority >= 2) {
                    /* Remove message from queue */
                    xQueueReceive(xNetworkQueue, &msg, 0);
                    
                    /* Prepare data for encryption */
                    snprintf(status_msg, sizeof(status_msg),
                            "%.2f|%u|%d|%d",
                            msg.data.value, (unsigned int)msg.data.timestamp,
                            msg.data.type, msg.data.sensor_id);
                    
                    /* Encrypt the data */
                    if (encrypt_data((uint8_t*)status_msg, strlen(status_msg),
                                   encrypted_buffer, &encrypted_len) == 0) {
                        
                        /* Sign the encrypted data */
                        if (sign_data(encrypted_buffer, encrypted_len, &signature) == 0) {
                            
                            /* Update message and put back in queue */
                            msg.encrypted = true;
                            xQueueSendToBack(xNetworkQueue, &msg, pdMS_TO_TICKS(100));
                            
                            safe_printf("[Security] Encrypted and signed message for %s sensor %d (sig: 0x%08x)\n",
                                      msg.data.type == SENSOR_TYPE_TEMPERATURE ? "temp" :
                                      msg.data.type == SENSOR_TYPE_HUMIDITY ? "humidity" : "motion",
                                      msg.data.sensor_id, (unsigned int)signature);
                        }
                    }
                }
            }
        }
        
        /* Periodic status report */
        static TickType_t last_report = 0;
        if ((xTaskGetTickCount() - last_report) > pdMS_TO_TICKS(30000)) {
            safe_printf("[Security] Stats - Encrypted: %u, Signed: %u, Keys Rotated: %u, Errors: %u\n",
                       (unsigned int)sec_ctx.stats.messages_encrypted,
                       (unsigned int)sec_ctx.stats.messages_signed,
                       (unsigned int)sec_ctx.stats.key_rotations,
                       (unsigned int)sec_ctx.stats.security_errors);
            last_report = xTaskGetTickCount();
        }
        
        /* Check for shutdown event */
        EventBits_t events = xEventGroupGetBits(xSystemEvents);
        if (events & EVENT_SHUTDOWN) {
            safe_printf("[Security] Shutting down\n");
            break;
        }
        
        /* Yield to other tasks */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    vTaskDelete(NULL);
}