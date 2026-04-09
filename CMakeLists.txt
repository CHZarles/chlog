cmake_minimum_required(VERSION 3.14)

# ---------------------------------------------------------------------------
# Library — usable via add_subdirectory(chlog) or find_package(chlog)
# ---------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands for clangd" FORCE)
endif()

project(chlog LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CHLOG_BUILD_EXAMPLES "Build examples" OFF)
option(CHLOG_ENABLE_CLANGD_INDEX "Add a lightweight target so clangd can index public headers" ON)

add_library(chlog STATIC
    src/sink.cpp
)
add_library(chlog::sink ALIAS chlog)

target_include_directories(chlog PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND CHLOG_ENABLE_CLANGD_INDEX)
    set(CHLOG_CLANGD_INDEX_FILE "${CMAKE_CURRENT_BINARY_DIR}/chlog_clangd_index.cpp")
    file(WRITE "${CHLOG_CLANGD_INDEX_FILE}" "#include \"sink.h\"\n")

    add_library(chlog_clangd_index OBJECT EXCLUDE_FROM_ALL "${CHLOG_CLANGD_INDEX_FILE}")
    target_link_libraries(chlog_clangd_index PRIVATE chlog::sink)
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if(CHLOG_BUILD_EXAMPLES)
    include(FetchContent)
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 10.2.1
    )
    FetchContent_MakeAvailable(fmt)

    add_executable(demo examples/demo.cpp)
    target_link_libraries(demo PRIVATE fmt::fmt chlog::sink)
endif()
