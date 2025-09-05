#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "config.h"
#include "common.h"
#include <errno.h>

/* MQTT Protocol Constants */
#define MQTT_PROTOCOL_LEVEL     4
#define MQTT_CONNECT            0x10
#define MQTT_CONNACK            0x20
#define MQTT_PUBLISH            0x30
#define MQTT_PUBACK             0x40
#define MQTT_SUBSCRIBE          0x80
#define MQTT_SUBACK             0x90
#define MQTT_PINGREQ            0xC0
#define MQTT_PINGRESP           0xD0
#define MQTT_DISCONNECT         0xE0

#define MQTT_QOS0               0x00
#define MQTT_QOS1               0x02
#define MQTT_RETAIN             0x01

#define MQTT_KEEPALIVE_SEC      60
#define MQTT_BUFFER_SIZE        1024

/* Network state */
typedef enum {
    NET_STATE_DISCONNECTED,
    NET_STATE_CONNECTING,
    NET_STATE_MQTT_CONNECT,
    NET_STATE_CONNECTED,
    NET_STATE_ERROR
} network_state_t;

/* MQTT connection context (simplified without TLS) */
typedef struct {
    int socket_fd;
    network_state_t state;
    uint16_t packet_id;
    TickType_t last_ping_time;
    uint8_t tx_buffer[MQTT_BUFFER_SIZE];
    uint8_t rx_buffer[MQTT_BUFFER_SIZE];
} mqtt_context_t;

static mqtt_context_t mqtt_ctx;

/* MQTT packet creation helpers */
static uint16_t mqtt_encode_length(uint8_t *buf, uint32_t length) {
    uint16_t encoded_bytes = 0;
    
    do {
        uint8_t encoded_byte = length % 128;
        length /= 128;
        if (length > 0) {
            encoded_byte |= 0x80;
        }
        buf[encoded_bytes++] = encoded_byte;
    } while (length > 0);
    
    return encoded_bytes;
}

static int mqtt_create_connect_packet(uint8_t *buf, size_t buf_size) {
    uint8_t *ptr = buf;
    uint16_t client_id_len = strlen(MQTT_CLIENT_ID);
    uint16_t payload_len = 2 + client_id_len;
    uint16_t variable_header_len = 10;  /* Protocol name + level + flags + keepalive */
    
    /* Fixed header */
    *ptr++ = MQTT_CONNECT;
    ptr += mqtt_encode_length(ptr, variable_header_len + payload_len);
    
    /* Variable header - Protocol name */
    *ptr++ = 0x00;
    *ptr++ = 0x04;
    memcpy(ptr, "MQTT", 4);
    ptr += 4;
    
    /* Protocol level */
    *ptr++ = MQTT_PROTOCOL_LEVEL;
    
    /* Connect flags (Clean session) */
    *ptr++ = 0x02;
    
    /* Keep alive */
    *ptr++ = (MQTT_KEEPALIVE_SEC >> 8) & 0xFF;
    *ptr++ = MQTT_KEEPALIVE_SEC & 0xFF;
    
    /* Payload - Client ID */
    *ptr++ = (client_id_len >> 8) & 0xFF;
    *ptr++ = client_id_len & 0xFF;
    memcpy(ptr, MQTT_CLIENT_ID, client_id_len);
    ptr += client_id_len;
    
    return ptr - buf;
}

static int mqtt_create_publish_packet(uint8_t *buf, size_t buf_size, 
                                     const char *topic, const uint8_t *payload, 
                                     size_t payload_len, uint8_t qos) {
    uint8_t *ptr = buf;
    uint16_t topic_len = strlen(topic);
    uint16_t variable_header_len = 2 + topic_len;
    
    if (qos > 0) {
        variable_header_len += 2;  /* Packet ID */
    }
    
    /* Fixed header */
    *ptr++ = MQTT_PUBLISH | (qos << 1);
    ptr += mqtt_encode_length(ptr, variable_header_len + payload_len);
    
    /* Variable header - Topic */
    *ptr++ = (topic_len >> 8) & 0xFF;
    *ptr++ = topic_len & 0xFF;
    memcpy(ptr, topic, topic_len);
    ptr += topic_len;
    
    /* Packet ID for QoS > 0 */
    if (qos > 0) {
        mqtt_ctx.packet_id++;
        *ptr++ = (mqtt_ctx.packet_id >> 8) & 0xFF;
        *ptr++ = mqtt_ctx.packet_id & 0xFF;
    }
    
    /* Payload */
    memcpy(ptr, payload, payload_len);
    ptr += payload_len;
    
    return ptr - buf;
}

static int mqtt_create_ping_packet(uint8_t *buf) {
    buf[0] = MQTT_PINGREQ;
    buf[1] = 0x00;
    return 2;
}

/* Initialize TCP connection (without TLS) */
static int init_tcp_connection(void) {
    struct sockaddr_in server_addr;
    struct hostent *server;
    
    safe_printf("[Network] Connecting to %s:%d (TCP only)...\n", 
                MQTT_BROKER_ADDRESS, MQTT_BROKER_PORT);
    
    /* Create socket */
    mqtt_ctx.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (mqtt_ctx.socket_fd < 0) {
        safe_printf("[Network] Failed to create socket\n");
        return -1;
    }
    
    /* Get server address */
    server = gethostbyname(MQTT_BROKER_ADDRESS);
    if (server == NULL) {
        safe_printf("[Network] Failed to resolve hostname: %s\n", MQTT_BROKER_ADDRESS);
        close(mqtt_ctx.socket_fd);
        return -1;
    }
    
    /* Setup server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    server_addr.sin_port = htons(1883);  /* Use non-TLS port */
    
    /* Connect to server */
    if (connect(mqtt_ctx.socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        safe_printf("[Network] Failed to connect to server\n");
        close(mqtt_ctx.socket_fd);
        return -1;
    }
    
    safe_printf("[Network] TCP connection established\n");
    xEventGroupSetBits(xSystemEvents, EVENT_NETWORK_CONNECTED);
    
    return 0;
}

/* Send MQTT packet over TCP */
static int mqtt_send_packet(const uint8_t *packet, size_t len) {
    ssize_t sent = send(mqtt_ctx.socket_fd, packet, len, 0);
    if (sent < 0) {
        safe_printf("[Network] Failed to send packet\n");
        return -1;
    }
    return sent;
}

/* Process incoming MQTT packets */
static void process_mqtt_packet(uint8_t *packet, size_t len) {
    uint8_t packet_type = packet[0] & 0xF0;
    
    switch (packet_type) {
        case MQTT_CONNACK:
            if (len >= 4 && packet[3] == 0x00) {
                safe_printf("[Network] MQTT connected successfully\n");
                mqtt_ctx.state = NET_STATE_CONNECTED;
                xEventGroupSetBits(xSystemEvents, EVENT_MQTT_CONNECTED);
            } else {
                safe_printf("[Network] MQTT connection rejected: %02x\n", packet[3]);
                mqtt_ctx.state = NET_STATE_ERROR;
            }
            break;
            
        case MQTT_PUBACK:
            safe_printf("[Network] PUBACK received\n");
            break;
            
        case MQTT_PINGRESP:
            /* Ping response received */
            break;
            
        default:
            safe_printf("[Network] Unknown packet type: 0x%02x\n", packet_type);
            break;
    }
}

void vNetworkTask(void *pvParameters) {
    (void)pvParameters;  /* Suppress unused parameter warning */
    
    message_t msg;
    char topic[128];
    char payload[256];
    ssize_t bytes_received;
    
    safe_printf("[Network] Started (TCP mode - no TLS)\n");
    
    /* Initialize network state */
    mqtt_ctx.state = NET_STATE_DISCONNECTED;
    mqtt_ctx.packet_id = 1;
    mqtt_ctx.socket_fd = -1;
    
    /* Wait for system to be ready */
    xEventGroupWaitBits(xSystemEvents, EVENT_DATA_READY, pdFALSE, pdTRUE, portMAX_DELAY);
    
    for (;;) {
        /* Handle connection state */
        if (mqtt_ctx.state == NET_STATE_DISCONNECTED) {
            /* Initialize TCP connection */
            if (init_tcp_connection() == 0) {
                /* Send MQTT CONNECT packet */
                int len = mqtt_create_connect_packet(mqtt_ctx.tx_buffer, MQTT_BUFFER_SIZE);
                if (mqtt_send_packet(mqtt_ctx.tx_buffer, len) > 0) {
                    mqtt_ctx.state = NET_STATE_MQTT_CONNECT;
                    mqtt_ctx.last_ping_time = xTaskGetTickCount();
                } else {
                    mqtt_ctx.state = NET_STATE_ERROR;
                }
            } else {
                mqtt_ctx.state = NET_STATE_ERROR;
            }
        }
        
        /* Check for incoming data */
        if (mqtt_ctx.state >= NET_STATE_MQTT_CONNECT && mqtt_ctx.socket_fd >= 0) {
            bytes_received = recv(mqtt_ctx.socket_fd, mqtt_ctx.rx_buffer, MQTT_BUFFER_SIZE, MSG_DONTWAIT);
            if (bytes_received > 0) {
                process_mqtt_packet(mqtt_ctx.rx_buffer, bytes_received);
            } else if (bytes_received < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                safe_printf("[Network] Connection lost\n");
                mqtt_ctx.state = NET_STATE_ERROR;
            }
        }
        
        /* Process outgoing messages if connected */
        if (mqtt_ctx.state == NET_STATE_CONNECTED) {
            if (xQueueReceive(xNetworkQueue, &msg, pdMS_TO_TICKS(100)) == pdPASS) {
                /* Format topic and payload based on sensor type */
                const char *sensor_type_str;
                switch (msg.data.type) {
                    case SENSOR_TYPE_TEMPERATURE:
                        sensor_type_str = "temperature";
                        break;
                    case SENSOR_TYPE_HUMIDITY:
                        sensor_type_str = "humidity";
                        break;
                    case SENSOR_TYPE_MOTION:
                        sensor_type_str = "motion";
                        break;
                    default:
                        sensor_type_str = "unknown";
                }
                
                /* Create topic */
                snprintf(topic, sizeof(topic), "%s%s/sensor_%d", 
                        MQTT_TOPIC_BASE, sensor_type_str, msg.data.sensor_id);
                
                /* Create JSON payload */
                snprintf(payload, sizeof(payload),
                        "{\"sensor_id\":%d,\"type\":\"%s\",\"value\":%.2f,"
                        "\"timestamp\":%u,\"priority\":%d,\"encrypted\":%s}",
                        msg.data.sensor_id, sensor_type_str, msg.data.value,
                        (unsigned int)msg.data.timestamp, msg.priority,
                        msg.encrypted ? "true" : "false");
                
                /* Create and send MQTT PUBLISH packet */
                int len = mqtt_create_publish_packet(mqtt_ctx.tx_buffer, MQTT_BUFFER_SIZE,
                                                   topic, (uint8_t*)payload, strlen(payload),
                                                   msg.priority > 1 ? MQTT_QOS1 : MQTT_QOS0);
                
                if (mqtt_send_packet(mqtt_ctx.tx_buffer, len) > 0) {
                    safe_printf("[Network] Published to %s: %.2f\n", 
                               topic, msg.data.value);
                } else {
                    safe_printf("[Network] Failed to publish message\n");
                    /* Put message back in queue */
                    xQueueSendToFront(xNetworkQueue, &msg, 0);
                    mqtt_ctx.state = NET_STATE_ERROR;
                }
            }
            
            /* Send periodic ping to keep connection alive */
            if ((xTaskGetTickCount() - mqtt_ctx.last_ping_time) > 
                pdMS_TO_TICKS(MQTT_KEEPALIVE_SEC * 1000 / 2)) {
                
                int len = mqtt_create_ping_packet(mqtt_ctx.tx_buffer);
                if (mqtt_send_packet(mqtt_ctx.tx_buffer, len) > 0) {
                    mqtt_ctx.last_ping_time = xTaskGetTickCount();
                } else {
                    mqtt_ctx.state = NET_STATE_ERROR;
                }
            }
        }
        
        /* Handle error state */
        if (mqtt_ctx.state == NET_STATE_ERROR) {
            safe_printf("[Network] Connection error, reconnecting in 5 seconds...\n");
            
            /* Clear connection events */
            xEventGroupClearBits(xSystemEvents, 
                               EVENT_NETWORK_CONNECTED | EVENT_MQTT_CONNECTED);
            
            /* Cleanup connection */
            if (mqtt_ctx.socket_fd >= 0) {
                close(mqtt_ctx.socket_fd);
                mqtt_ctx.socket_fd = -1;
            }
            
            /* Wait before reconnecting */
            vTaskDelay(pdMS_TO_TICKS(5000));
            
            /* Reset state */
            mqtt_ctx.state = NET_STATE_DISCONNECTED;
        }
        
        /* Check for shutdown event */
        EventBits_t events = xEventGroupGetBits(xSystemEvents);
        if (events & EVENT_SHUTDOWN) {
            safe_printf("[Network] Shutting down\n");
            
            /* Send DISCONNECT packet if connected */
            if (mqtt_ctx.state == NET_STATE_CONNECTED) {
                mqtt_ctx.tx_buffer[0] = MQTT_DISCONNECT;
                mqtt_ctx.tx_buffer[1] = 0x00;
                mqtt_send_packet(mqtt_ctx.tx_buffer, 2);
            }
            
            /* Cleanup */
            if (mqtt_ctx.socket_fd >= 0) {
                close(mqtt_ctx.socket_fd);
                mqtt_ctx.socket_fd = -1;
            }
            
            break;
        }
        
        /* Small delay to prevent tight loop */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    vTaskDelete(NULL);
}