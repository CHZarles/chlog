cmake_minimum_required(VERSION 3.14)
project(chlog LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands for clangd" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.12
)
FetchContent_MakeAvailable(doctest)


# ---------------------------------------------------------------------------
# Library — usable via add_subdirectory(chlog) or find_package(chlog)
# ---------------------------------------------------------------------------
add_library(chlog STATIC
    src/chlog.cpp
    src/format.cpp
    src/sink.cpp
    src/queue.cpp
)
add_library(chlog::sink ALIAS chlog)

target_include_directories(chlog PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(chlog PUBLIC fmt::fmt)

include(CTest)
if(BUILD_TESTING)
    add_executable(chlog_tests
        tests/test_main.cpp
        tests/test_sync_log.cpp
        tests/rotating_file_sink_test.cpp
        tests/format_common_test.cpp
        tests/queue_test.cpp
    )
    target_link_libraries(chlog_tests PRIVATE chlog::sink doctest::doctest)
    add_test(NAME chlog_tests COMMAND chlog_tests)
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
option(CHLOG_BUILD_EXAMPLES "Build examples" OFF)
if(CHLOG_BUILD_EXAMPLES)
    add_executable(demo examples/demo.cpp)
    target_link_libraries(demo PRIVATE fmt::fmt chlog::sink)
endif()
