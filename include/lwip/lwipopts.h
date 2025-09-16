#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#define LWIP_COMPAT_MUTEX               0
#define LWIP_COMPAT_MUTEX_ALLOWED       0
#define LWIP_TCPIP_CORE_LOCKING         0
#define LWIP_MPU_COMPATIBLE             0
#define SYS_LIGHTWEIGHT_PROT            1
#define MEM_LIBC_MALLOC                 1
#define MEMP_MEM_MALLOC                 1
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (32 * 1024)
#define MEMP_NUM_PBUF                   32
#define MEMP_NUM_UDP_PCB                8
#define MEMP_NUM_TCP_PCB                16
#define MEMP_NUM_TCP_PCB_LISTEN         8
#define MEMP_NUM_TCP_SEG                32
#define MEMP_NUM_SYS_TIMEOUT            16
#define MEMP_NUM_NETBUF                 8
#define MEMP_NUM_NETCONN                16
#define PBUF_POOL_SIZE                  32
#define PBUF_POOL_BUFSIZE               1536
#define LWIP_TCP                        1
#define TCP_TTL                         255
#define TCP_QUEUE_OOSEQ                 1
#define TCP_MSS                         1460
#define TCP_SND_BUF                     (8 * TCP_MSS)
#define TCP_SND_QUEUELEN                (4 * TCP_SND_BUF/TCP_MSS)
#define TCP_WND                         (4 * TCP_MSS)
#define TCP_MAXRTX                      12
#define TCP_SYNMAXRTX                   6
#define LWIP_UDP                        1
#define UDP_TTL                         255
#define LWIP_ICMP                       1
#define LWIP_DHCP                       1
#define LWIP_AUTOIP                     0
#define LWIP_DNS                        1
#define DNS_TABLE_SIZE                  4
#define DNS_MAX_NAME_LENGTH             256
#define IP_FORWARD                      0
#define IP_OPTIONS_ALLOWED              1
#define IP_REASSEMBLY                   1
#define IP_FRAG                         1
#define IP_DEFAULT_TTL                  255
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1
#define LWIP_NETIF_HOSTNAME             1
#define LWIP_NETIF_API                  1
#define LWIP_NETIF_LOOPBACK             1
#define LWIP_LOOPBACK_MAX_PBUFS         8
#define LWIP_SOCKET                     1
#define LWIP_TCP_KEEPALIVE              1
#define LWIP_SO_SNDTIMEO                1
#define LWIP_SO_RCVTIMEO                1
#define LWIP_SO_RCVBUF                  1
#define SO_REUSE                        1
#define SO_REUSE_RXTOALL                1
#define LWIP_STATS                      1
#define LWIP_STATS_DISPLAY              1
#define LINK_STATS                      1
#define IP_STATS                        1
#define ICMP_STATS                      1
#define UDP_STATS                       1
#define TCP_STATS                       1
#define MEM_STATS                       1
#define MEMP_STATS                      1
#define SYS_STATS                       1
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_GEN_TCP                1
#define CHECKSUM_GEN_ICMP               1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_TCP              1
#define CHECKSUM_CHECK_ICMP             1
#define LWIP_DEBUG                      0
#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_ALL
#define LWIP_DBG_TYPES_ON               LWIP_DBG_ON
#define LWIP_NETCONN                    1
#define LWIP_SOCKET_SET_ERRNO           1
#define TCPIP_THREAD_NAME               "lwIP"
#define TCPIP_THREAD_STACKSIZE          4096
#define TCPIP_THREAD_PRIO               3
#define TCPIP_MBOX_SIZE                 16
#define DEFAULT_THREAD_STACKSIZE        2048
#define DEFAULT_THREAD_PRIO             1
#define DEFAULT_RAW_RECVMBOX_SIZE       16
#define DEFAULT_UDP_RECVMBOX_SIZE       16
#define DEFAULT_TCP_RECVMBOX_SIZE       16
#define DEFAULT_ACCEPTMBOX_SIZE         16
#define LWIP_TIMERS                     1
#define LWIP_TIMEVAL_PRIVATE            0
#define NO_SYS                          0
#define LWIP_PROVIDE_ERRNO              1
#define LWIP_IPV6                       0
#define LWIP_IGMP                       1
#define LWIP_RAW                        1
#define LWIP_SNMP                       0
#define LWIP_NETBIOS                    0
#define LWIP_TFTP                       0
#define sys_mutex_new               sys_arch_mutex_new
#define sys_mutex_free              sys_arch_mutex_free
#define sys_mutex_lock              sys_arch_mutex_lock
#define sys_mutex_unlock            sys_arch_mutex_unlock
#define sys_mutex_valid             sys_arch_mutex_valid
#define sys_mutex_set_invalid       sys_arch_mutex_set_invalid

#endif /* LWIPOPTS_H */