#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

/* Architecture specific includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

/* Define platform endianness */
#define BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN

/* Compiler hints for packing structures */
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

/* Platform specific diagnostic output */
#define LWIP_PLATFORM_DIAG(x) do {printf x;} while(0)
#define LWIP_PLATFORM_ASSERT(x) do {printf("Assertion \"%s\" failed at line %d in %s\n", \
                                     x, __LINE__, __FILE__); fflush(NULL); abort();} while(0)

/* Define random number generator */
#define LWIP_RAND() ((u32_t)rand())

/* Memory alignment */
#define MEM_ALIGNMENT 4

/* Critical section protection (can be empty for single threaded) */
#define SYS_ARCH_DECL_PROTECT(lev)
#define SYS_ARCH_PROTECT(lev)
#define SYS_ARCH_UNPROTECT(lev)

/* Checksum calculation can be done by CPU */
#define LWIP_CHKSUM_ALGORITHM 2

#endif /* LWIP_ARCH_CC_H */