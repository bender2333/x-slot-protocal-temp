# X-Slot 无线互联协议完整工程
# 目录结构：
# xslot/
# ├── CMakeLists.txt              (根CMake文件)
# ├── README.md                   (工程说明)
# ├── src/                        (源代码目录)
# │   ├── CMakeLists.txt
# │   ├── core/                   (核心协议层)
# │   │   ├── xslot_protocol.h
# │   │   ├── xslot_protocol.cpp
# │   │   ├── xslot_codec.h
# │   │   ├── xslot_codec.cpp
# │   │   └── xslot_manager.cpp
# │   ├── transport/              (传输层)
# │   │   ├── xslot_transport.h
# │   │   ├── xslot_at_transport.h
# │   │   ├── xslot_at_transport.cpp
# │   │   ├── xslot_uart_transport.h
# │   │   └── xslot_uart_transport.cpp
# │   ├── bacnet/                 (BACnet序列化)
# │   │   ├── xslot_bacnet.h
# │   │   └── xslot_bacnet.cpp
# │   ├── hal/                    (硬件抽象层)
# │   │   ├── xslot_hal.h
# │   │   ├── xslot_hal_win32.cpp
# │   │   └── xslot_hal_freertos.cpp
# │   └── xslot_c_api.cpp         (C接口导出)
# ├── include/                    (公共头文件)
# │   ├── xslot.h                 (C接口)
# │   └── xslot.hpp               (C++接口)
# ├── demo/                       (示例程序)
# │   ├── CMakeLists.txt
# │   ├── demo_basic.cpp
# │   ├── demo_gateway.cpp
# │   └── demo_edge_node.cpp
# ├── doc/                        (文档)
# │   ├── architecture.md
# │   ├── protocol_spec.md
# │   └── api_reference.md
# └── test/                       (测试代码)
#     └── test_protocol.cpp

# ============================================================================
# 根CMakeLists.txt
# ============================================================================

cmake_minimum_required(VERSION 3.15)
project(XSlotProtocol VERSION 1.0.0 LANGUAGES C CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

# 编译选项
option(XSLOT_BUILD_SHARED "Build shared library" OFF)
option(XSLOT_BUILD_DEMO "Build demo applications" ON)
option(XSLOT_BUILD_TESTS "Build test suite" OFF)
option(XSLOT_PLATFORM_FREERTOS "Target FreeRTOS platform" OFF)

# 平台检测
if(WIN32)
    set(XSLOT_PLATFORM "WIN32")
elseif(XSLOT_PLATFORM_FREERTOS)
    set(XSLOT_PLATFORM "FREERTOS")
else()
    set(XSLOT_PLATFORM "POSIX")
endif()

message(STATUS "XSlot Platform: ${XSLOT_PLATFORM}")

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 子目录
add_subdirectory(src)

if(XSLOT_BUILD_DEMO)
    add_subdirectory(demo)
endif()

if(XSLOT_BUILD_TESTS)
    add_subdirectory(test)
endif()

# 安装规则
install(DIRECTORY include/ DESTINATION include)
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    DESTINATION .
)

# ============================================================================
# src/CMakeLists.txt
# ============================================================================

# 核心源文件
set(XSLOT_CORE_SOURCES
    core/xslot_protocol.cpp
    core/xslot_codec.cpp
    core/xslot_manager.cpp
    transport/xslot_at_transport.cpp
    bacnet/xslot_bacnet.cpp
    xslot_c_api.cpp
)

# 平台相关源文件
if(XSLOT_PLATFORM STREQUAL "WIN32")
    list(APPEND XSLOT_CORE_SOURCES 
        hal/xslot_hal_win32.cpp
        transport/xslot_uart_win32.cpp
    )
elseif(XSLOT_PLATFORM STREQUAL "FREERTOS")
    list(APPEND XSLOT_CORE_SOURCES 
        hal/xslot_hal_freertos.cpp
        transport/xslot_uart_freertos.cpp
    )
endif()

# 创建库
if(XSLOT_BUILD_SHARED)
    add_library(xslot SHARED ${XSLOT_CORE_SOURCES})
else()
    add_library(xslot STATIC ${XSLOT_CORE_SOURCES})
endif()

# 链接库
if(WIN32)
    target_link_libraries(xslot PRIVATE ws2_32)
endif()

# 设置输出目录
set_target_properties(xslot PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 安装库
install(TARGETS xslot
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# ============================================================================
# demo/CMakeLists.txt
# ============================================================================

# 基础通信示例
add_executable(demo_basic demo_basic.cpp)
target_link_libraries(demo_basic xslot)

# 网关节点示例
add_executable(demo_gateway demo_gateway.cpp)
target_link_libraries(demo_gateway xslot)

# 边缘节点示例
add_executable(demo_edge_node demo_edge_node.cpp)
target_link_libraries(demo_edge_node xslot)

# 设置输出目录
set_target_properties(
    demo_basic demo_gateway demo_edge_node
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/demo"
)

# 安装示例
install(TARGETS demo_basic demo_gateway demo_edge_node
    RUNTIME DESTINATION bin/demo
)