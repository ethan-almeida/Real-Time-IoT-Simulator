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
#include <fcntl.h>
#include <sys/select.h>
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "mbedtls/timing.h"
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


typedef enum {
    NET_STATE_DISCONNECTED,
    NET_STATE_CONNECTING,
    NET_STATE_MQTT_CONNECT,
    NET_STATE_CONNECTED,
    NET_STATE_ERROR
} network_state_t;


typedef struct {
    int socket_fd;
    network_state_t state;
    uint16_t packet_id;
    TickType_t last_ping_time;
    uint8_t tx_buffer[MQTT_BUFFER_SIZE];
    uint8_t rx_buffer[MQTT_BUFFER_SIZE];
    mbedtls_ssl_context ssl_ctx;
    mbedtls_ssl_config ssl_conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509_crt cacert;
} mqtt_context_t;

static mqtt_context_t mqtt_ctx;

static int is_connection_alive(void) {
    if (mqtt_ctx.socket_fd < 0) {
        return 0;
    }
    int error = 0;
    socklen_t len = sizeof(error);
    int retval = getsockopt(mqtt_ctx.socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);
    
    if (retval != 0) {
        safe_printf("Network getsockopt failed: %s\n", strerror(errno));
        return 0;
    }
    
    if (error != 0) {
        safe_printf("Network Socket error detected: %s\n", strerror(error));
        return 0;
    }

    fd_set readfds;
    struct timeval tv = {0, 0};
    FD_ZERO(&readfds);
    FD_SET(mqtt_ctx.socket_fd, &readfds);
    int sel = select(mqtt_ctx.socket_fd + 1, &readfds, NULL, NULL, &tv);
    if (sel < 0 && errno != EINTR) {
        safe_printf("Network Select failed in health check: %s\n", strerror(errno));
        return 0;
    }
    
    return 1;
}

static void debug_certificate_verification(void) {
    uint32_t flags = mbedtls_ssl_get_verify_result(&mqtt_ctx.ssl_ctx);
    
    if (flags != 0) {
        char vrfy_buf[512];
        safe_printf("Network Certificate verification flags: 0x%08x\n", flags);
        
        if (flags & MBEDTLS_X509_BADCERT_EXPIRED)
            safe_printf("Network   - Certificate expired\n");
        if (flags & MBEDTLS_X509_BADCERT_REVOKED)
            safe_printf("Network   - Certificate revoked\n");
        if (flags & MBEDTLS_X509_BADCERT_CN_MISMATCH)
            safe_printf("Network   - CN mismatch (expected: %s)\n", MQTT_BROKER_ADDRESS);
        if (flags & MBEDTLS_X509_BADCERT_NOT_TRUSTED)
            safe_printf("Network   - Certificate not trusted\n");
        if (flags & MBEDTLS_X509_BADCRL_NOT_TRUSTED)
            safe_printf("Network   - CRL not trusted\n");
        if (flags & MBEDTLS_X509_BADCRL_EXPIRED)
            safe_printf("Network   - CRL expired\n");
        if (flags & MBEDTLS_X509_BADCERT_OTHER)
            safe_printf("Network   - Other certificate issue\n");
        if (flags & MBEDTLS_X509_BADCERT_FUTURE)
            safe_printf("Network   - Certificate validity starts in the future\n");
        if (flags & MBEDTLS_X509_BADCRL_FUTURE)
            safe_printf("Network   - CRL validity starts in the future\n");
        if (flags & MBEDTLS_X509_BADCERT_KEY_USAGE)
            safe_printf("Network   - Key usage violation\n");
        if (flags & MBEDTLS_X509_BADCERT_EXT_KEY_USAGE)
            safe_printf("Network   - Extended key usage violation\n");
        if (flags & MBEDTLS_X509_BADCERT_NS_CERT_TYPE)
            safe_printf("Network   - NS certificate type violation\n");
        if (flags & MBEDTLS_X509_BADCERT_BAD_MD)
            safe_printf("Network   - Bad message digest\n");
        if (flags & MBEDTLS_X509_BADCERT_BAD_PK)
            safe_printf("Network   - Bad public key\n");
        if (flags & MBEDTLS_X509_BADCERT_BAD_KEY)
            safe_printf("Network   - Bad key\n");
        if (flags & MBEDTLS_X509_BADCRL_BAD_MD)
            safe_printf("Network   - Bad CRL message digest\n");
        if (flags & MBEDTLS_X509_BADCRL_BAD_PK)
            safe_printf("Network   - Bad CRL public key\n");
        if (flags & MBEDTLS_X509_BADCRL_BAD_KEY)
            safe_printf("Network   - Bad CRL key\n");
            
        mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
        safe_printf("Network Full verification info:\n%s\n", vrfy_buf);
    }
    
    const mbedtls_x509_crt *peer_cert = mbedtls_ssl_get_peer_cert(&mqtt_ctx.ssl_ctx);
    if (peer_cert != NULL) {
        char cert_buf[2048];
        mbedtls_x509_crt_info(cert_buf, sizeof(cert_buf), "  ", peer_cert);
        //safe_printf("Network Peer certificate:\n%s\n", cert_buf);
    }
}

static uint16_t mqtt_encode_length(uint8_t *buf, uint32_t length) {
    uint16_t encoded_bytes = 0;
    do {
        uint8_t encoded_byte = length % 128;
        length /= 128;
        if (length > 0) {
            encoded_byte |= 128;
        }
        buf[encoded_bytes++] = encoded_byte;
    } while (length > 0);
    return encoded_bytes;
}

static int mqtt_create_connect_packet(uint8_t *buf, size_t buf_size) {
    uint8_t *ptr = buf;
    uint16_t client_id_len = strlen(MQTT_CLIENT_ID);
    *ptr++ = MQTT_CONNECT;
    uint32_t remaining_length = 10 + 2 + client_id_len; 
    ptr += mqtt_encode_length(ptr, remaining_length);
    *ptr++ = 0x00;
    *ptr++ = 0x04;
    memcpy(ptr, "MQTT", 4);
    ptr += 4;
    *ptr++ = MQTT_PROTOCOL_LEVEL;
    *ptr++ = 0x02;
    *ptr++ = (MQTT_KEEPALIVE_SEC >> 8) & 0xFF;
    *ptr++ = MQTT_KEEPALIVE_SEC & 0xFF;
    *ptr++ = (client_id_len >> 8) & 0xFF;
    *ptr++ = client_id_len & 0xFF;
    memcpy(ptr, MQTT_CLIENT_ID, client_id_len);
    ptr += client_id_len;
    safe_printf("Network CONNECT packet created, size: %ld bytes\n", ptr - buf);
    return ptr - buf;
}


static int mqtt_create_publish_packet(uint8_t *buf, size_t buf_size, 
                                     const char *topic, const uint8_t *payload, 
                                     size_t payload_len, uint8_t qos) {
    uint8_t *ptr = buf;
    uint16_t topic_len = strlen(topic);
    uint16_t variable_header_len = 2 + topic_len;
    
    if (qos > 0) {
        variable_header_len += 2; 
    }

    *ptr++ = MQTT_PUBLISH | (qos << 1);
    ptr += mqtt_encode_length(ptr, variable_header_len + payload_len);
    *ptr++ = (topic_len >> 8) & 0xFF;
    *ptr++ = topic_len & 0xFF;
    memcpy(ptr, topic, topic_len);
    ptr += topic_len;
    
    if (qos > 0) {
        mqtt_ctx.packet_id++;
        *ptr++ = (mqtt_ctx.packet_id >> 8) & 0xFF;
        *ptr++ = mqtt_ctx.packet_id & 0xFF;
    }
    
    memcpy(ptr, payload, payload_len);
    ptr += payload_len;
    return ptr - buf;
}


static int mqtt_create_ping_packet(uint8_t *buf) {
    buf[0] = MQTT_PINGREQ;
    buf[1] = 0x00;
    return 2;
}


static void mbedtls_debug_callback(void *ctx, int level, const char *file, int line, const char *str) {
    ((void) level);
    safe_printf("%s:%04d: %s", file, line, str);
}


static int my_mbedtls_send(void *ctx, const unsigned char *buf, size_t len) {
    int fd = *(int *)ctx;
    int ret = send(fd, buf, len, 0);
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        }
        return -1;
    }
    return ret;
}

static int my_mbedtls_recv(void *ctx, unsigned char *buf, size_t len) {
    int fd = *(int *)ctx;
    int ret = recv(fd, buf, len, 0);
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
        return -1;
    }
    return ret;
}

static int init_mbedtls(void) {
    int ret;
    const char *pers = "iot_gateway_client";
    mbedtls_ssl_init(&mqtt_ctx.ssl_ctx);
    mbedtls_ssl_config_init(&mqtt_ctx.ssl_conf);
    mbedtls_entropy_init(&mqtt_ctx.entropy);
    mbedtls_ctr_drbg_init(&mqtt_ctx.ctr_drbg);
    mbedtls_x509_crt_init(&mqtt_ctx.cacert);
    
    if ((ret = mbedtls_ctr_drbg_seed(&mqtt_ctx.ctr_drbg, mbedtls_entropy_func, 
                                     &mqtt_ctx.entropy, (const unsigned char *) pers, strlen(pers))) != 0) {
        safe_printf("Network Failed to seed the random number generator: -0x%x\n", -ret);
        return -1;
    }

    const char *ca_cert_path = "/home/stickman/Real-Time-IoT-Simulator/mosquitto.org.crt";

    if (access(ca_cert_path, R_OK) != 0) {
        safe_printf("Network CA certificate file not found, trying alternative...\n");
        ca_cert_path = "/home/stickman/Real-Time-IoT-Simulator/lets-encrypt-r3.pem";
        if (access(ca_cert_path, R_OK) != 0) {
            safe_printf("Network No CA certificate file found\n");
            return -1;
        }
    }
    
    safe_printf("Network Loading CA certificate from: %s\n", ca_cert_path);
    if ((ret = mbedtls_x509_crt_parse_file(&mqtt_ctx.cacert, ca_cert_path)) != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        safe_printf("Network Failed to parse CA certificate: -0x%x (%s)\n", -ret, error_buf);
        return -1;
    }
    
    safe_printf("Network CA certificate loaded successfully\n");
    if ((ret = mbedtls_ssl_config_defaults(&mqtt_ctx.ssl_conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        safe_printf("Network Failed to set SSL/TLS defaults: -0x%x\n", -ret);
        return -1;
    }
    
    mbedtls_ssl_conf_rng(&mqtt_ctx.ssl_conf, mbedtls_ctr_drbg_random, &mqtt_ctx.ctr_drbg);
    mbedtls_ssl_conf_dbg(&mqtt_ctx.ssl_conf, mbedtls_debug_callback, NULL);
    
    mbedtls_debug_set_threshold(1); //debug level -> 0,1,2,3; higher number for more debug info
    
    mbedtls_ssl_conf_authmode(&mqtt_ctx.ssl_conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&mqtt_ctx.ssl_conf, &mqtt_ctx.cacert, NULL);
    mbedtls_ssl_conf_read_timeout(&mqtt_ctx.ssl_conf, 10000);
    if ((ret = mbedtls_ssl_setup(&mqtt_ctx.ssl_ctx, &mqtt_ctx.ssl_conf)) != 0) {
        safe_printf("Network Failed to set up SSL context: -0x%x\n", -ret);
        return -1;
    }

    if ((ret = mbedtls_ssl_set_hostname(&mqtt_ctx.ssl_ctx, MQTT_BROKER_ADDRESS)) != 0) {
        safe_printf("Network Failed to set hostname: -0x%x\n", -ret);
        return -1;
    }
    
    return 0;
}


static int init_tls_connection(void) {
    int ret;
    
    if (init_mbedtls() != 0) {
        safe_printf("Network Failed to initialize mbedTLS\n");
        return -1;
    }
    
    mqtt_ctx.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (mqtt_ctx.socket_fd < 0) {
        safe_printf("Network Failed to create socket\n");
        return -1;
    }
    
    int flags = fcntl(mqtt_ctx.socket_fd, F_GETFL, 0);
    fcntl(mqtt_ctx.socket_fd, F_SETFL, flags | O_NONBLOCK);
    
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(MQTT_BROKER_ADDRESS, NULL, &hints, &res) != 0) {
        safe_printf("Network Failed to resolve hostname\n");
        close(mqtt_ctx.socket_fd);
        return -1;
    }

    struct sockaddr_in *server_addr_ptr = (struct sockaddr_in *)res->ai_addr;
    server_addr_ptr->sin_port = htons(MQTT_BROKER_PORT);
    safe_printf("Network Connecting to %s:%d (TLS mode)\n", MQTT_BROKER_ADDRESS, MQTT_BROKER_PORT);
    
    int connect_res = connect(mqtt_ctx.socket_fd, (struct sockaddr *)server_addr_ptr, sizeof(*server_addr_ptr));
    
    if (connect_res < 0) {
        if (errno == EINPROGRESS) {
            fd_set writefds;
            struct timeval timeout;
            int sel;
            do {
                FD_ZERO(&writefds);
                FD_SET(mqtt_ctx.socket_fd, &writefds);
                timeout.tv_sec = 10;
                timeout.tv_usec = 0;
                
                sel = select(mqtt_ctx.socket_fd + 1, NULL, &writefds, NULL, &timeout);
            } while (sel < 0 && errno == EINTR);
            if (sel > 0 && FD_ISSET(mqtt_ctx.socket_fd, &writefds)) {
                int optval;
                socklen_t optlen = sizeof(optval);
                if (getsockopt(mqtt_ctx.socket_fd, SOL_SOCKET, SO_ERROR, &optval, &optlen) == 0 && optval == 0) {
                    connect_res = 0;  
                } else {
                    safe_printf("Network Connection failed: %s\n", strerror(optval));
                    connect_res = -1;
                }
            } else if (sel == 0) {
                safe_printf("Network Connection timeout\n");
                connect_res = -1;
            } else {
                safe_printf("Network Select error: %s\n", strerror(errno));
                connect_res = -1;
            }
        } else {
            safe_printf("Network Connect error: %s\n", strerror(errno));
            connect_res = -1;
        }
    }

    freeaddrinfo(res);
    
    if (connect_res < 0) {
        close(mqtt_ctx.socket_fd);
        return -1;
    }
    
    safe_printf("Network TCP connection established\n");
    mbedtls_ssl_set_bio(&mqtt_ctx.ssl_ctx, &mqtt_ctx.socket_fd, 
                        my_mbedtls_send, my_mbedtls_recv, NULL);
    
    safe_printf("Network Starting SSL handshake...\n");
    int handshake_attempts = 0;
    const int max_attempts = 50;

    while ((ret = mbedtls_ssl_handshake(&mqtt_ctx.ssl_ctx)) != 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(100));
            handshake_attempts++;
            if (handshake_attempts > max_attempts) {
                safe_printf("Network SSL handshake timeout\n");
                close(mqtt_ctx.socket_fd);
                return -1;
            }
            continue;
        }
        
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        safe_printf("Network SSL handshake failed: -0x%x (%s)\n", -ret, error_buf);
        close(mqtt_ctx.socket_fd);
        return -1;
    }
    
    safe_printf("Network SSL handshake successful\n");
    debug_certificate_verification();
    
    uint32_t verify_flags;
    if ((verify_flags = mbedtls_ssl_get_verify_result(&mqtt_ctx.ssl_ctx)) != 0) {
    char vrfy_buf[512];
    mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", verify_flags);
    safe_printf("Network Certificate verification failed:\n%s\n", vrfy_buf);
    
    if (TLS_VERIFY_REQUIRED) {
        safe_printf("Network Continuing despite verification failure (debug mode)\n");
        close(mqtt_ctx.socket_fd);
        return -1;
    }
}
    
    return 0;
}


static int mqtt_send_packet(const uint8_t *packet, size_t len) {
    int ret;
    size_t written = 0;
    int attempts = 0;
    const int max_attempts = 50; 
    
    while (written < len) {
        ret = mbedtls_ssl_write(&mqtt_ctx.ssl_ctx, packet + written, len - written);
        
        if (ret > 0) {
            written += ret;
            attempts = 0; 
        } else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(100));
            attempts++;
            if (attempts > max_attempts) {
                safe_printf("Network Send timeout\n");
                return -1;
            }
        } else {
            char error_buf[100];
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            safe_printf("Network Failed to send packet: -0x%x (%s)\n", -ret, error_buf);
            return -1;
        }
    }
    
    return written;
}


static void cleanup_tls_connection(void) {
    mbedtls_ssl_close_notify(&mqtt_ctx.ssl_ctx);
    if (mqtt_ctx.socket_fd >= 0) {
        close(mqtt_ctx.socket_fd);
        mqtt_ctx.socket_fd = -1;
    }
    mbedtls_ssl_free(&mqtt_ctx.ssl_ctx);
    mbedtls_ssl_config_free(&mqtt_ctx.ssl_conf);
    mbedtls_ctr_drbg_free(&mqtt_ctx.ctr_drbg);
    mbedtls_entropy_free(&mqtt_ctx.entropy);
    mbedtls_x509_crt_free(&mqtt_ctx.cacert);
}

static void process_mqtt_packet(uint8_t *packet, size_t len) {
    if (len < 2) {
        safe_printf("Network Packet too short: %zu bytes\n", len);
        return;
    }
    
    uint8_t packet_type = packet[0] & 0xF0;
    safe_printf("Network Received packet type: 0x%02x, length: %zu\n", packet_type, len);
    
    switch (packet_type) {
        case MQTT_CONNACK:
            if (len >= 4) {
                uint8_t connect_return_code = packet[3];
                if (connect_return_code == 0x00) {
                    safe_printf("Network MQTT connected successfully\n");
                    mqtt_ctx.state = NET_STATE_CONNECTED;
                    xEventGroupSetBits(xSystemEvents, EVENT_MQTT_CONNECTED);
                } else {
                    safe_printf("Network MQTT connection rejected, return code: 0x%02x\n", connect_return_code);
                    switch(connect_return_code) {
                        case 0x01: safe_printf("Network   Reason: Unacceptable protocol version\n"); break;
                        case 0x02: safe_printf("Network   Reason: Identifier rejected\n"); break;
                        case 0x03: safe_printf("Network   Reason: Server unavailable\n"); break;
                        case 0x04: safe_printf("Network   Reason: Bad username or password\n"); break;
                        case 0x05: safe_printf("Network   Reason: Not authorized\n"); break;
                        default: safe_printf("Network   Reason: Unknown (0x%02x)\n", connect_return_code); break;
                    }
                    mqtt_ctx.state = NET_STATE_ERROR;
                }
            } else {
                safe_printf("Network CONNACK packet too short\n");
                mqtt_ctx.state = NET_STATE_ERROR;
            }
            break;
            
        case MQTT_PUBACK:
            if (len >= 4) {
                uint16_t packet_id = (packet[2] << 8) | packet[3];
                safe_printf("Network PUBACK received for packet ID: %u\n", packet_id);
            }
            break;
            
        case MQTT_PINGRESP:
            safe_printf("Network PINGRESP received\n");
            break;
            
        default:
            safe_printf("Network Unknown packet type: 0x%02x\n", packet_type);
            safe_printf("Network Packet dump: ");
            for (size_t i = 0; i < len && i < 16; i++) {
                safe_printf("%02x ", packet[i]);
            }
            safe_printf("\n");
            break;
    }
}

void vNetworkTask(void *pvParameters) {
    (void)pvParameters; 
    
    message_t msg;
    char topic[128];
    char payload[256];
    int reconnect_attempts = 0;
    const int max_reconnect_attempts = 5;
    
    safe_printf("Network Started (TLS mode)\n");
    mqtt_ctx.state = NET_STATE_DISCONNECTED;
    mqtt_ctx.packet_id = 1;
    mqtt_ctx.socket_fd = -1;
    printf("Network Waiting for system ready event...\n");
    xEventGroupWaitBits(xSystemEvents, EVENT_DATA_READY, pdFALSE, pdTRUE, portMAX_DELAY);
    printf("Network System ready event received!\n");
    printf("Network Initializing network interface...\n");
    printf("Network Entering main loop...\n");
    
    for (;;) {
        if (mqtt_ctx.state == NET_STATE_DISCONNECTED) {
            if (reconnect_attempts >= max_reconnect_attempts) {
                safe_printf("Network Max reconnection attempts reached, waiting longer...\n");
                vTaskDelay(pdMS_TO_TICKS(30000)); 
                reconnect_attempts = 0;
            }
            
            if (init_tls_connection() == 0) {
                int len = mqtt_create_connect_packet(mqtt_ctx.tx_buffer, MQTT_BUFFER_SIZE);
                if (mqtt_send_packet(mqtt_ctx.tx_buffer, len) > 0) {
                    mqtt_ctx.state = NET_STATE_MQTT_CONNECT;
                    mqtt_ctx.last_ping_time = xTaskGetTickCount();
                    reconnect_attempts = 0; 
                    safe_printf("Network MQTT CONNECT packet sent\n");
                } else {
                    safe_printf("Network Failed to send MQTT CONNECT\n");
                    mqtt_ctx.state = NET_STATE_ERROR;
                }
            } else {
                reconnect_attempts++;
                mqtt_ctx.state = NET_STATE_ERROR;
            }
        }
        
        if (mqtt_ctx.state >= NET_STATE_MQTT_CONNECT && !is_connection_alive()) {
            safe_printf("Network Connection health check failed\n");
            mqtt_ctx.state = NET_STATE_ERROR;
        }
        
        if (mqtt_ctx.state >= NET_STATE_MQTT_CONNECT) {
            int ret = mbedtls_ssl_read(&mqtt_ctx.ssl_ctx, mqtt_ctx.rx_buffer, MQTT_BUFFER_SIZE);
            if (ret > 0) {
                process_mqtt_packet(mqtt_ctx.rx_buffer, ret);
            } else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            } else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                safe_printf("Network Peer closed connection gracefully\n");
                mqtt_ctx.state = NET_STATE_ERROR;
            } else if (ret < 0) {
                char error_buf[100];
                mbedtls_strerror(ret, error_buf, sizeof(error_buf));
                safe_printf("Network Read error: -0x%x (%s)\n", -ret, error_buf);
                mqtt_ctx.state = NET_STATE_ERROR;
            }
        }
        
        if (mqtt_ctx.state == NET_STATE_CONNECTED) {
            if (xQueueReceive(xNetworkQueue, &msg, pdMS_TO_TICKS(100)) == pdPASS) {
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
                
                snprintf(topic, sizeof(topic), "%s%s/sensor_%d", 
                        MQTT_TOPIC_BASE, sensor_type_str, msg.data.sensor_id);

                snprintf(payload, sizeof(payload),
                        "{\"sensor_id\":%d,\"type\":\"%s\",\"value\":%.2f,"
                        "\"timestamp\":%u,\"priority\":%d,\"encrypted\":%s}",
                        msg.data.sensor_id, sensor_type_str, msg.data.value,
                        (unsigned int)msg.data.timestamp, msg.priority,
                        msg.encrypted ? "true" : "false");
                
                int len = mqtt_create_publish_packet(mqtt_ctx.tx_buffer, MQTT_BUFFER_SIZE, topic, (uint8_t*)payload, strlen(payload), msg.priority > 1 ? MQTT_QOS1 : MQTT_QOS0);
                
                if (mqtt_send_packet(mqtt_ctx.tx_buffer, len) > 0) {
                    safe_printf("Network Published to %s: %.2f\n", 
                               topic, msg.data.value);
                } else {
                    safe_printf("Network Failed to publish message\n");
                    if (xQueueSendToFront(xNetworkQueue, &msg, 0) != pdPASS) {
                        safe_printf("Network Failed to requeue message\n");
                    }
                    mqtt_ctx.state = NET_STATE_ERROR;
                }
            }
    
            if ((xTaskGetTickCount() - mqtt_ctx.last_ping_time) > 
                pdMS_TO_TICKS(MQTT_KEEPALIVE_SEC * 1000 / 2)) {
                
                int len = mqtt_create_ping_packet(mqtt_ctx.tx_buffer);
                if (mqtt_send_packet(mqtt_ctx.tx_buffer, len) > 0) {
                    mqtt_ctx.last_ping_time = xTaskGetTickCount();
                    safe_printf("Network PING sent\n");
                } else {
                    safe_printf("Network Failed to send PING\n");
                    mqtt_ctx.state = NET_STATE_ERROR;
                }
            }
        }
        
        if (mqtt_ctx.state == NET_STATE_ERROR) {
            safe_printf("Network Connection error, cleaning up and reconnecting...\n");
            xEventGroupClearBits(xSystemEvents, 
                               EVENT_NETWORK_CONNECTED | EVENT_MQTT_CONNECTED);
            
            cleanup_tls_connection();
            int delay_ms = 5000 + (reconnect_attempts * 2000); 

            if (delay_ms > 30000) {
                delay_ms = 30000; 
            }

            safe_printf("Network Waiting %d seconds before reconnect attempt %d\n", delay_ms/1000, reconnect_attempts + 1);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            mqtt_ctx.state = NET_STATE_DISCONNECTED;
        }
        
        EventBits_t events = xEventGroupGetBits(xSystemEvents);
        if (events & EVENT_SHUTDOWN) {
            safe_printf("Network shutting down\n");

            if (mqtt_ctx.state == NET_STATE_CONNECTED) {
                mqtt_ctx.tx_buffer[0] = MQTT_DISCONNECT;
                mqtt_ctx.tx_buffer[1] = 0x00;
                mqtt_send_packet(mqtt_ctx.tx_buffer, 2);
            }
            cleanup_tls_connection();
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(NULL);
}