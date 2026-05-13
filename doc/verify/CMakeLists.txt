cmake_minimum_required(VERSION 3.20)
project(verify_cpp VERSION 1.0.0 LANGUAGES CXX C)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Linux only (current: ${CMAKE_SYSTEM_NAME}).")
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Requires Clang. Current: ${CMAKE_CXX_COMPILER_ID}")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(DEBUG_MODE "Debug: -O0 -g" OFF)

set(CMAKE_CXX_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_C_FLAGS_DEBUG "")

if(DEBUG_MODE)
    message(STATUS "Build mode: DEBUG (-O0 -g)")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O0 -g")
else()
    message(STATUS "Build mode: RELEASE (-O2)")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -march=native")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O2 -march=native")
endif()

add_compile_options(-Wall -Wextra -fcolor-diagnostics -fansi-escape-codes)

# 复用主 cpp 目录里的 yyjson (单文件 lib)
set(YYJSON_DIR ${CMAKE_SOURCE_DIR}/../../cpp/include/package/yyjson)
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${YYJSON_DIR}/../..
)

set(SOURCES
    src/main.cpp
    src/common/fs.cpp
    src/common/csv.cpp
    src/common/bq.cpp
    src/common/ts_loader.cpp
    src/common/diff.cpp
    src/verifier/calendar.cpp
    src/verifier/stk_limit.cpp
    src/verifier/suspend_d.cpp
    src/verifier/margin_secs.cpp
    src/verifier/margin_detail.cpp
    src/verifier/stock_st.cpp
    src/verifier/adj_factor.cpp
    src/verifier/daily_basic_close.cpp
    src/verifier/daily_basic_shares.cpp
    ${YYJSON_DIR}/yyjson.c
)

add_executable(verify_cpp ${SOURCES})
target_link_libraries(verify_cpp PRIVATE pthread)

message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}, std=${CMAKE_CXX_STANDARD}")
message(STATUS "yyjson:   ${YYJSON_DIR}")
message(STATUS "Output:   ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
