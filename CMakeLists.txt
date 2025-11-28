# =============================================================================
# X-Slot Protocol - CMake 构建配置
# =============================================================================
cmake_minimum_required(VERSION 3.15)
project(xslot VERSION 1.0.0 LANGUAGES C CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# 编译选项
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 平台检测
if(WIN32)
    set(XSLOT_PLATFORM "windows")
elseif(UNIX AND NOT APPLE)
    set(XSLOT_PLATFORM "linux")
else()
    set(XSLOT_PLATFORM "unknown")
endif()

message(STATUS "X-Slot Platform: ${XSLOT_PLATFORM}")

# 选项
option(XSLOT_BUILD_DEMO "Build demo applications" ON)
option(XSLOT_BUILD_TEST "Build unit tests" OFF)

# =============================================================================
# 库目标
# =============================================================================
add_library(xslot STATIC
    # Core
    src/core/xslot_manager.cpp
    src/core/xslot_protocol.cpp
    src/core/message_codec.cpp
    src/core/node_table.cpp
    
    # Transport
    src/transport/tpmesh_transport.cpp
    src/transport/tpmesh_at_driver.cpp
    src/transport/direct_transport.cpp
    src/transport/null_transport.cpp
    
    # BACnet
    src/bacnet/bacnet_serializer.cpp
    src/bacnet/bacnet_incremental.cpp
    
    # C API
    src/xslot_c_api.cpp
)

# HAL 层 (平台相关)
if(XSLOT_PLATFORM STREQUAL "windows")
    target_sources(xslot PRIVATE src/hal/hal_windows.cpp)
elseif(XSLOT_PLATFORM STREQUAL "linux")
    target_sources(xslot PRIVATE src/hal/hal_linux.cpp)
endif()

# 头文件路径
target_include_directories(xslot
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/bacnet
)

# Windows 链接库
if(WIN32)
    target_link_libraries(xslot PRIVATE ws2_32)
endif()

# Linux 链接库
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(xslot PRIVATE Threads::Threads)
endif()

# =============================================================================
# Demo 应用
# =============================================================================
if(XSLOT_BUILD_DEMO)
    add_executable(demo_edge_node demo/demo_edge_node.cpp)
    target_link_libraries(demo_edge_node PRIVATE xslot)
    
    add_executable(demo_hub_node demo/demo_hub_node.cpp)
    target_link_libraries(demo_hub_node PRIVATE xslot)
    
    add_executable(demo_hmi demo/demo_hmi.cpp)
    target_link_libraries(demo_hmi PRIVATE xslot)
endif()

# =============================================================================
# 单元测试
# =============================================================================
if(XSLOT_BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif()

# =============================================================================
# 安装配置
# =============================================================================
install(TARGETS xslot
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/xslot
    DESTINATION include
)

