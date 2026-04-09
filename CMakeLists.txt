cmake_minimum_required(VERSION 3.14)
project(chlog LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands for clangd" FORCE)
endif()


# ---------------------------------------------------------------------------
# Library — usable via add_subdirectory(chlog) or find_package(chlog)
# ---------------------------------------------------------------------------
add_library(chlog STATIC
    src/sink.cpp
)
add_library(chlog::sink ALIAS chlog)

target_include_directories(chlog PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
option(CHLOG_BUILD_EXAMPLES "Build examples" OFF)
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
