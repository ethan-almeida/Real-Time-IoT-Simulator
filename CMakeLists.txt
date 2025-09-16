cmake_minimum_required(VERSION 3.10)
project(IoTGatewaySimulator C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -DNDEBUG")

# Platform detection
if(UNIX AND NOT APPLE)
    set(PLATFORM_NAME "Linux")
    add_definitions(-DPLATFORM_LINUX)
elseif(APPLE)
    set(PLATFORM_NAME "macOS")
    add_definitions(-DPLATFORM_MACOS)
elseif(WIN32)
    set(PLATFORM_NAME "Windows")
    add_definitions(-DPLATFORM_WINDOWS)
endif()

message(STATUS "Building for ${PLATFORM_NAME}")

# Library paths
set(FREERTOS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/freertos)
set(FREERTOS_SOURCE_DIR ${FREERTOS_DIR}/Source)
set(FREERTOS_INCLUDE_DIR ${FREERTOS_SOURCE_DIR}/include)
set(FREERTOS_PORT_DIR ${FREERTOS_SOURCE_DIR}/portable/ThirdParty/GCC/Posix)

set(MBEDTLS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/mbedTLS)
set(MBEDTLS_INCLUDE_DIR ${MBEDTLS_DIR}/include)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${FREERTOS_INCLUDE_DIR}
    ${FREERTOS_PORT_DIR}
    ${FREERTOS_PORT_DIR}/utils
    ${MBEDTLS_INCLUDE_DIR}
)

add_definitions(-DLWIP_NO_STDINT_H=1 -DLWIP_NO_STDIO_H=1)

set_source_files_properties(${LWIP_SOURCES} PROPERTIES COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/include")

set(ENABLE_TESTING OFF)
set(ENABLE_PROGRAMS OFF)
add_subdirectory(${MBEDTLS_DIR} EXCLUDE_FROM_ALL)

# FreeRTOS sources
set(FREERTOS_SOURCES
    ${FREERTOS_SOURCE_DIR}/tasks.c
    ${FREERTOS_SOURCE_DIR}/queue.c
    ${FREERTOS_SOURCE_DIR}/list.c
    ${FREERTOS_SOURCE_DIR}/timers.c
    ${FREERTOS_SOURCE_DIR}/event_groups.c
    ${FREERTOS_SOURCE_DIR}/stream_buffer.c
    ${FREERTOS_SOURCE_DIR}/heap_1.c
    ${FREERTOS_PORT_DIR}/port.c
    ${FREERTOS_PORT_DIR}/utils/wait_for_event.c
)

# Application sources
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tasks/sensors.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tasks/network.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tasks/security.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sys_arch.c
)


add_executable(iot_gateway_sim ${APP_SOURCES} ${FREERTOS_SOURCES} ${LWIP_SOURCES})

target_link_libraries(iot_gateway_sim pthread mbedtls mbedx509 mbedcrypto)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(iot_gateway_sim rt)
endif()


target_link_libraries(iot_gateway_sim m)


install(TARGETS iot_gateway_sim DESTINATION bin)


add_custom_target(run
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/iot_gateway_sim
    DEPENDS iot_gateway_sim
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)


message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C Flags: ${CMAKE_C_FLAGS}")


get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
message(STATUS "=== Include Directories ===")
foreach(dir ${dirs})
  message(STATUS "Include dir: ${dir}")
endforeach()