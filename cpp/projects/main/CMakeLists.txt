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
    message(STATUS "Build mode: PRODUCTION (-O3 -DNDEBUG)")
    # NOTE: -ffast-math 不可加 — 代码全程靠 std::nanf("") 标记缺失值, 与 -ffinite-math-only 冲突会 SIGSEGV
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -march=native")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DNDEBUG -march=native")
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
# per-strategy {backtest::run → analysis::run} → report::aggregate → write_meta.
# 输出 <git_root>/output/{meta.json, strategy/<name>/{backtest/*.npy +
# labels.json + report.json, analysis/*.npy + report.json},
# aggregate/{*.npy, report.json}}; py/report.py 直读生成报告.
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
    ../../src/feature/tensor.cpp
    ../../src/feature/pit.cpp
    ../../src/feature/load.cpp
    ../../src/feature/ts.cpp
    ../../src/feature/cs.cpp
    ../../src/feature/build.cpp
    ../../src/feature/describe.cpp
    ../../src/feature/report.cpp
    ../../src/strategy/columns.cpp
    ../../src/backtest/engine.cpp
    ../../src/backtest/backtest.cpp
    ../../src/mine/mine.cpp
    ../../src/analysis/analysis.cpp
    ../../src/report/metrics.cpp
    ../../src/report/json.cpp
    ../../src/report/labels.cpp
    ../../src/report/aggregate.cpp
    ../../include/package/yyjson/yyjson.c
)

set(HEADERS
    ../../include/config_main.hpp
    ../../include/package/yyjson/yyjson.h
    ../../include/misc/date.hpp
    ../../include/misc/fs.hpp
    ../../include/misc/parquet.hpp
    ../../include/misc/schedule.hpp
    ../../include/misc/timer.hpp
    ../../include/misc/affinity.hpp
    ../../include/misc/logging.hpp
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
    ../../include/feature/graph.hpp
    ../../include/feature/registry.hpp
    ../../include/feature/tensor.hpp
    ../../include/feature/pit.hpp
    ../../include/feature/load.hpp
    ../../include/feature/ts.hpp
    ../../include/feature/cs.hpp
    ../../include/feature/build.hpp
    ../../include/feature/describe.hpp
    ../../include/strategy/strategy.hpp
    ../../include/strategy/registry.hpp
    ../../include/strategy/columns.hpp
    ../../include/backtest/engine.hpp
    ../../include/backtest/backtest.hpp
    ../../include/config_mine.hpp
    ../../include/mine/mine.hpp
    ../../include/analysis/analysis.hpp
    ../../include/report/metrics.hpp
    ../../include/report/json.hpp
    ../../include/report/labels.hpp
    ../../include/report/aggregate.hpp
)

# feature/def/{basic,factor,filter}/*.hpp 与 strategy/def/*.hpp 是 header-only
#   节点定义 (一节点一文件, 文件名 == 节点名), 无需在此手动逐条挂载 — 加新节点
#   只需写文件本身 + 在依赖它的节点/策略 config 里 #include, C++ 层的依赖解析/
#   注册完全由 graph.hpp consteval 完成. 下面几份 glob 结果多份复用:
#   1) 挂到 add_executable 给 IDE 看 2) 生成 feature/def/all.hpp 全量清单
#   3) basic/ 那份还用来生成 feature/framework_roots.hpp (= FRAMEWORK_ROOTS,
#   basic/ 目录内容本身就是框架固定根的定义, 见 registry.hpp 顶部说明).
#   CONFIGURE_DEPENDS: 新增/删除 def 文件后重新 configure 即自动同步.
file(GLOB FEATURE_DEF_BASIC_FILES CONFIGURE_DEPENDS
    ../../include/feature/def/basic/*.hpp
)
file(GLOB FEATURE_DEF_FACTOR_FILES CONFIGURE_DEPENDS
    ../../include/feature/def/factor/*.hpp
)
file(GLOB FEATURE_DEF_FILTER_FILES CONFIGURE_DEPENDS
    ../../include/feature/def/filter/*.hpp
)
list(SORT FEATURE_DEF_BASIC_FILES)
list(SORT FEATURE_DEF_FACTOR_FILES)
list(SORT FEATURE_DEF_FILTER_FILES)
set(FEATURE_DEF_SPEC_FILES
    ${FEATURE_DEF_BASIC_FILES} ${FEATURE_DEF_FACTOR_FILES} ${FEATURE_DEF_FILTER_FILES})
file(GLOB FEATURE_DEF_MISC_HEADERS
    ../../include/feature/def/*.hpp
)
set(FEATURE_DEF_HEADERS ${FEATURE_DEF_MISC_HEADERS} ${FEATURE_DEF_SPEC_FILES})
file(GLOB STRATEGY_DEF_HEADERS
    ../../include/strategy/def/*.hpp
)

# 把一份 FeatureSpec 文件列表拼成 "#include ...\n" / "    &def::<name>_spec,\n"
# 两份文本 (按约定 "文件名 == 节点名 == FeatureSpec 变量名前缀" 推导符号名).
function(qmt_feature_spec_lines files out_includes out_roots)
    set(includes "")
    set(roots "")
    foreach(spec_file ${files})
        get_filename_component(spec_name ${spec_file} NAME_WE)
        file(RELATIVE_PATH spec_rel_path ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${spec_file})
        string(APPEND includes "#include \"${spec_rel_path}\"  // IWYU pragma: export\n")
        string(APPEND roots "    &def::${spec_name}_spec,\n")
    endforeach()
    set(${out_includes} "${includes}" PARENT_SCOPE)
    set(${out_roots} "${roots}" PARENT_SCOPE)
endfunction()

# ============================================================================
# feature/framework_roots.hpp (生成到 build 目录) — FRAMEWORK_ROOTS = basic/
#   目录下全部节点 (框架自身固定根, 与策略配置无关, 见 registry.hpp 顶部说明).
#   registry.hpp #include 这份生成文件替代手写数组.
# ============================================================================
qmt_feature_spec_lines("${FEATURE_DEF_BASIC_FILES}" FRAMEWORK_ROOTS_INCLUDES FRAMEWORK_ROOTS_ENTRIES)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated/feature/framework_roots.hpp
"#pragma once

// AUTO-GENERATED by cpp/projects/main/CMakeLists.txt — 不要手动编辑.
// FRAMEWORK_ROOTS = feature/def/basic/ 目录下的全部节点 (框架自身固定根, 与
// 策略配置无关, 任何策略下都需要, 见 feature/registry.hpp 顶部说明). 新增/
// 删除 basic/*.hpp 后重新 cmake configure 即自动同步.

${FRAMEWORK_ROOTS_INCLUDES}
#include \"feature/graph.hpp\"

namespace feature::registry_detail {

inline constexpr const FeatureSpec *FRAMEWORK_ROOTS[] = {
${FRAMEWORK_ROOTS_ENTRIES}};

} // namespace feature::registry_detail
")

# ============================================================================
# feature/def/all.hpp (生成到 build 目录) — 供 feature/report.cpp 打印完整依赖
#   树用, 含未被任何策略引用的节点; 不参与 feature/registry.hpp 的真正计算图
#   构建/裁剪.
# ============================================================================
qmt_feature_spec_lines("${FEATURE_DEF_SPEC_FILES}" FEATURE_DEF_ALL_INCLUDES FEATURE_DEF_ALL_ROOTS)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated/feature/def/all.hpp
"#pragma once

// AUTO-GENERATED by cpp/projects/main/CMakeLists.txt — 不要手动编辑.
// 全量特征清单, 仅供 feature/report.cpp 打印完整依赖树用 (含未被任何策略引用
// 的节点); 不参与真正的计算图构建 (那部分由 feature/registry.hpp 的可达性裁
// 剪完成, 与本文件无关). 新增/删除 feature/def/{basic,factor,filter}/*.hpp
// 后重新 cmake configure 即自动同步.

${FEATURE_DEF_ALL_INCLUDES}
#include \"feature/graph.hpp\"

#include <array>
#include <cstddef>

namespace feature::registry_detail {

inline constexpr const FeatureSpec *const ALL_DEFINED_ROOTS[] = {
${FEATURE_DEF_ALL_ROOTS}};

inline constexpr std::size_t N_ALL_DEFINED = count_reachable(ALL_DEFINED_ROOTS);
inline constexpr std::array<const FeatureSpec *, N_ALL_DEFINED> ALL_DEFINED_NODES =
    collect_reachable<N_ALL_DEFINED>(ALL_DEFINED_ROOTS);

} // namespace feature::registry_detail
")

# ============================================================================
# strategy/registry_generated.hpp (生成到 build 目录) — STRATEGIES[] 挂载表.
#   自动扫描 strategy/def/*.hpp (约定 "文件名 == 子 namespace 名, spec 变量固定
#   叫 spec"), 生成 includes + STRATEGIES[] 数组. 每个策略文件用独立子 namespace
#   隔离通用辅助变量名 (name/filters/weights/...), 用户只需加减 strategy/def/
#   <name>.hpp, 重新 cmake configure 即自动挂载, 无需手动改其他文件.
# ============================================================================
list(SORT STRATEGY_DEF_HEADERS)
set(STRAT_INCLUDES "")
set(STRAT_ENTRIES "")
foreach(strat_file ${STRATEGY_DEF_HEADERS})
    get_filename_component(strat_name ${strat_file} NAME_WE)
    file(RELATIVE_PATH strat_rel_path ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${strat_file})
    string(APPEND STRAT_INCLUDES "#include \"${strat_rel_path}\"  // IWYU pragma: export\n")
    string(APPEND STRAT_ENTRIES "    &def::${strat_name}::spec,\n")
endforeach()
list(LENGTH STRATEGY_DEF_HEADERS STRAT_N)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated/strategy/registry_generated.hpp
"#pragma once

// AUTO-GENERATED by cpp/projects/main/CMakeLists.txt — 不要手动编辑.
// STRATEGIES[] = strategy/def/*.hpp 全部策略 spec (约定 文件名 == 子 namespace
// 名, spec 变量固定叫 spec). 新增/删除 strategy/def/*.hpp 后重新 cmake
// configure 即自动同步.
// consteval 校验 (名字唯一 / filters 全 Filter / weights 全 Factor 且 w!=0 /
// 参数域合法) 仍在 strategy/registry.hpp 里做, 违规编译失败.

${STRAT_INCLUDES}#include \"strategy/strategy.hpp\"

#include <array>

namespace strategy {

inline constexpr std::array<const StrategySpec *, ${STRAT_N}> STRATEGIES = {{
${STRAT_ENTRIES}}};

} // namespace strategy
")

include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)

add_executable(app_main ${SOURCES} ${HEADERS} ${FEATURE_DEF_HEADERS} ${STRATEGY_DEF_HEADERS})

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
