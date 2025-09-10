#ifndef CONFIG_H
#define CONFIG_H

#define SYS_NAME "Embedded Gateway"

#define SENSOR_TASK_STACK_SIZE      (2048)
#define PROCESSOR_TASK_STACK_SIZE   (2048)
#define NETWORK_TASK_STACK_SIZE     (1024)
#define SECURITY_TASK_STACK_SIZE    (2048)
#define MONITOR_TASK_STACK_SIZE     (1024)

/* Queue Sizes */
#define SENSOR_QUEUE_LENGTH         (10)
#define NETWORK_QUEUE_LENGTH        (5)
#define MAX_MESSAGE_SIZE            (256)

/* Network Configuration */
#define MQTT_BROKER_ADDRESS         "test.mosquitto.org"
#define MQTT_BROKER_PORT            1883
#define MQTT_CLIENT_ID              "stick_gateway"
#define MQTT_TOPIC_BASE             "iot/gateway/"

/* Sensor Configuration */
#define NUM_TEMP_SENSORS            3
#define NUM_HUMIDITY_SENSORS        2
#define NUM_MOTION_SENSORS          1
#define SENSOR_READ_INTERVAL_MS     1000

/* Security Configuration */
#define TLS_VERIFY_REQUIRED         1
#define MAX_CERT_SIZE               4096

#endif 