cmake_minimum_required(VERSION 3.20)
project(Main VERSION 1.0.0 LANGUAGES CXX C)

# ============================================================================
# Toolchain (Linux only, Clang required)
# ============================================================================
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "This project supports Linux only (current: ${CMAKE_SYSTEM_NAME}).")
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "This project requires Clang. Current compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================================
# Build Mode Options
# ============================================================================
option(PROFILE_MODE "Profile mode: -O2 -g, Tracy instrumentation" OFF)
option(DEBUG_MODE "Debug mode: -O0 -g" OFF)
option(ASSERT_MODE "Assert mode: -O2 with assertions enabled" OFF)

set(CMAKE_CXX_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
set(CMAKE_CXX_FLAGS_MINSIZEREL "")
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "")
set(CMAKE_C_FLAGS_MINSIZEREL "")

if(DEBUG_MODE)
    message(STATUS "Build mode: DEBUG (-O0 -g, core-dump friendly)")
    add_compile_definitions(DEBUG_MODE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -march=native")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -march=native")

elseif(PROFILE_MODE)
    message(STATUS "Build mode: PROFILE (Tracy instrumentation)")
    add_compile_definitions(PROFILE_MODE TRACY_ENABLE TRACY_NO_SYSTEM_TRACING TRACY_NO_SAMPLING)
    # NOTE: -ffast-math 不可加 — 代码全程靠 std::nanf("") 标记缺失值, 与 -ffinite-math-only 冲突会 SIGSEGV
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g -march=native")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g -march=native")

elseif(ASSERT_MODE)
    message(STATUS "Build mode: ASSERT (-O2 + asserts)")
    add_compile_definitions(ASSERT_MODE)
    # NOTE: -ffast-math 不可加 — 代码全程靠 std::nanf("") 标记缺失值, 与 -ffinite-math-only 冲突会 SIGSEGV
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -march=native")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -march=native")

else()
    message(STATUS "Build mode: PRODUCTION (-O2 -DNDEBUG)")
    # NOTE: -ffast-math 不可加 — 代码全程靠 std::nanf("") 标记缺失值, 与 -ffinite-math-only 冲突会 SIGSEGV
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -DNDEBUG -march=native")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -DNDEBUG -march=native")
endif()

add_compile_options(-fcolor-diagnostics -fansi-escape-codes -Wno-overriding-option)

# ============================================================================
# Includes — 含 vendored Arrow C++ Flight headers (cpp/include/package/arrow/include)
# ============================================================================
set(ARROW_VENDOR ${CMAKE_CURRENT_SOURCE_DIR}/../../include/package/arrow)
set(ARROW_INCLUDE ${ARROW_VENDOR}/include)
set(ARROW_LIB ${ARROW_VENDOR}/lib)

if(NOT EXISTS ${ARROW_INCLUDE}/arrow/flight/client.h)
    message(FATAL_ERROR
        "Vendored Arrow headers missing: ${ARROW_INCLUDE}/arrow/flight/client.h\n"
        "Run: ${ARROW_VENDOR}/vendor_from_pyarrow.sh")
endif()
if(NOT EXISTS ${ARROW_INCLUDE}/parquet/arrow/reader.h)
    message(FATAL_ERROR
        "Vendored Parquet headers missing: ${ARROW_INCLUDE}/parquet/arrow/reader.h\n"
        "Run: ${ARROW_VENDOR}/vendor_from_pyarrow.sh")
endif()
if(NOT EXISTS ${ARROW_LIB}/libarrow_flight.so)
    message(FATAL_ERROR
        "Vendored Arrow shared libs missing: ${ARROW_LIB}/libarrow_flight.so\n"
        "Run: ${ARROW_VENDOR}/vendor_from_pyarrow.sh")
endif()
if(NOT EXISTS ${ARROW_LIB}/libparquet.so)
    message(FATAL_ERROR
        "Vendored Parquet shared lib missing: ${ARROW_LIB}/libparquet.so\n"
        "Run: ${ARROW_VENDOR}/vendor_from_pyarrow.sh")
endif()

include_directories(../../include)
# Arrow 13.x 头文件用 std::aligned_storage, 在 C++23 下被 deprecated.
# 用 SYSTEM include 抑制第三方头里的 -Wdeprecated-declarations 等警告.
include_directories(SYSTEM ${ARROW_INCLUDE})

# ============================================================================
# Source Files
# ============================================================================
# 完整闭环串到 main.cpp: bigquant::update → tushare::update → feature::build →
# backtest::run → analysis::run → write_meta. 输出 <git_root>/output/{meta.json,
# backtest/*.npy + labels.json, analysis/*.npy}; py/report.py 直读生成报告.
set(SOURCES
    ../../src/main.cpp
    ../../src/misc/date.cpp
    ../../src/misc/fs.cpp
    ../../src/misc/parquet.cpp
    ../../src/misc/schedule.cpp
    ../../src/misc/logging.cpp
    ../../src/misc/npy.cpp
    ../../src/misc/mmap.cpp
    ../../src/api/bigquant/dai.cpp
    ../../src/api/bigquant/spec.cpp
    ../../src/api/bigquant/pipeline.cpp
    ../../src/api/tushare/http.cpp
    ../../src/api/tushare/spec.cpp
    ../../src/api/tushare/parse.cpp
    ../../src/api/tushare/pipeline.cpp
    ../../src/feature/axis.cpp
    ../../src/feature/industry.cpp
    ../../src/feature/feature.cpp
    ../../src/feature/tensor.cpp
    ../../src/feature/pit.cpp
    ../../src/feature/load.cpp
    ../../src/feature/ts.cpp
    ../../src/feature/cs.cpp
    ../../src/feature/build.cpp
    ../../src/feature/describe.cpp
    ../../src/backtest/backtest.cpp
    ../../src/analysis/analysis.cpp
    ../../include/package/yyjson/yyjson.c
)

set(HEADERS
    ../../include/config.hpp
    ../../include/package/yyjson/yyjson.h
    ../../include/misc/date.hpp
    ../../include/misc/fs.hpp
    ../../include/misc/parquet.hpp
    ../../include/misc/schedule.hpp
    ../../include/misc/timer.hpp
    ../../include/misc/print.hpp
    ../../include/misc/progress_single.hpp
    ../../include/misc/progress_parallel.hpp
    ../../include/misc/affinity.hpp
    ../../include/misc/logging.hpp
    ../../include/misc/profiler.hpp
    ../../include/misc/npy.hpp
    ../../include/misc/mmap.hpp
    ../../include/api/bigquant/dai.hpp
    ../../include/api/bigquant/spec.hpp
    ../../include/api/bigquant/pipeline.hpp
    ../../include/api/tushare/http.hpp
    ../../include/api/tushare/spec.hpp
    ../../include/api/tushare/parse.hpp
    ../../include/api/tushare/pipeline.hpp
    ../../include/feature/axis.hpp
    ../../include/feature/industry.hpp
    ../../include/feature/feature.hpp
    ../../include/feature/tensor.hpp
    ../../include/feature/pit.hpp
    ../../include/feature/load.hpp
    ../../include/feature/ts.hpp
    ../../include/feature/cs.hpp
    ../../include/feature/build.hpp
    ../../include/feature/describe.hpp
    ../../include/backtest/backtest.hpp
    ../../include/analysis/analysis.hpp
)

add_executable(app_main ${SOURCES} ${HEADERS})

# vendored Arrow C++ Flight + Parquet + pthread (Flight 走明文 gRPC, 无 TLS 依赖)
target_link_libraries(app_main PRIVATE
    ${ARROW_LIB}/libarrow_flight.so
    ${ARROW_LIB}/libparquet.so
    ${ARROW_LIB}/libarrow.so
    pthread
    dl
)

# 运行时让 ld.so 找到 vendored libarrow*.so.1300
set_target_properties(app_main PROPERTIES
    BUILD_RPATH "${ARROW_LIB}"
    INSTALL_RPATH "${ARROW_LIB}"
)

# ============================================================================
# Install
# ============================================================================
install(TARGETS app_main RUNTIME DESTINATION bin)

add_custom_target(run_main
    COMMAND app_main
    DEPENDS app_main
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running app_main"
)

message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
