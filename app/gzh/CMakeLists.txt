cmake_minimum_required(VERSION 3.20)
project(wxmd LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------- packages
# 全部依赖以源码形式 vendored 在 packages/ 下，随本项目一起编译，不依赖系统包。

# lexbor: HTML5 解析 + CSS 选择器
set(LEXBOR_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LEXBOR_INSTALL_HEADERS OFF CACHE BOOL "" FORCE)
add_subdirectory(packages/lexbor EXCLUDE_FROM_ALL)

# quickjs-ng: JS 沙箱，用于执行微信文章内的 cgi 脚本
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_LIBC OFF CACHE BOOL "" FORCE)
add_subdirectory(packages/quickjs EXCLUDE_FROM_ALL)

# cpp-httplib (header-only) + nlohmann/json (header-only)
add_library(wxmd_headers INTERFACE)
target_include_directories(wxmd_headers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/packages/httplib
    ${CMAKE_CURRENT_SOURCE_DIR}/packages/json)

# cpp-httplib 的 HTTPS 支持走系统 OpenSSL
find_package(OpenSSL REQUIRED)
target_compile_definitions(wxmd_headers INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(wxmd_headers INTERFACE OpenSSL::SSL OpenSSL::Crypto)

# gzip：实测一篇正文 HTML 有 3.05MB，压完 0.68MB（省 78%），而它占了每篇下载
# 字节的 86% —— 图片本身已压缩，压不动。所以这一个开关就是三倍吞吐，值得多一个
# 系统依赖。定义了这个宏之后 httplib 自己发 Accept-Encoding 并自动解压。
find_package(ZLIB REQUIRED)
target_compile_definitions(wxmd_headers INTERFACE CPPHTTPLIB_ZLIB_SUPPORT)
target_link_libraries(wxmd_headers INTERFACE ZLIB::ZLIB)

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------- wxmd
add_library(wxmd STATIC
    src/dom.cpp
    src/html.cpp
    src/cgi.cpp
    src/renderer.cpp
    src/markdown.cpp
    src/egress.cpp
    src/fetch.cpp
    src/profile.cpp
    src/tlsca.cpp
    src/proxy.cpp
    src/capture.cpp
    src/desktop.cpp
    src/assets.cpp
    src/store.cpp
    src/sync.cpp
    src/wxmd.cpp)

target_include_directories(wxmd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(wxmd
    PUBLIC wxmd_headers
    PRIVATE lexbor_static qjs Threads::Threads)

# ----------------------------------------------------------------------- cli
# CLI 也用 src/ 下的内部头（config 的参数、strutil 的 trim），不必再抄一遍。
add_executable(wxmd_cli cli/main.cpp)
target_include_directories(wxmd_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(wxmd_cli PRIVATE wxmd)
set_target_properties(wxmd_cli PROPERTIES OUTPUT_NAME wxmd)