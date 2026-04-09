# chlog

一个简单的日志输出实验项目，当前代码里有 `Sink` / `ConsoleSink` 抽象，示例程序在 `examples/demo.cpp`。

## 编译 demo

当前仓库里的 `demo` 不是默认构建项，需要显式打开 `CHLOG_BUILD_EXAMPLES`：

```bash
cmake -S . -B build -DCHLOG_BUILD_EXAMPLES=ON
cmake --build build -j
```

如果后续链接成功，运行命令是：

```bash
./build/demo
```

注意：当前 `chlog` 还是 `INTERFACE` 目标，`src/sink.cpp` 没有参与构建，所以这两条命令目前会在链接阶段失败，提示 `ConsoleSink::write(...)` / `ConsoleSink::flush()` 未定义。要让 `demo` 真正编过，还需要先把实现文件接入 CMake。

## CMake 用法备忘

如果要把 `chlog` 从 `INTERFACE` 头文件库改成“可编译的静态库”，并且让 `sample/demo` 或别的工程链接它，推荐按下面的方式组织。

### 1. 把库改成静态库

核心思路是把 `src/sink.cpp` 编进 `chlog`，然后示例程序只链接库目标：

```cmake
cmake_minimum_required(VERSION 3.14)

project(chlog VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CHLOG_BUILD_EXAMPLES "Build examples" OFF)
option(CHLOG_INSTALL "Enable install rules" ON)

add_library(chlog STATIC
    src/sink.cpp
)
add_library(chlog::sink ALIAS chlog)

target_include_directories(chlog
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(chlog PUBLIC cxx_std_17)

if(CHLOG_BUILD_EXAMPLES)
    include(FetchContent)
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 10.2.1
    )
    FetchContent_MakeAvailable(fmt)

    add_executable(demo examples/demo.cpp)
    target_link_libraries(demo PRIVATE chlog::sink fmt::fmt)
endif()
```

注意：

- 一旦开始编译 `src/sink.cpp`，就不能再只停留在声明阶段；`ConsoleSink::write()` 和 `ConsoleSink::flush()` 需要有真正实现。
- `target_include_directories(... PUBLIC ...)` 让链接到 `chlog` 的上层目标自动继承 `include/` 头文件目录。
- `chlog::sink` 是别名目标，供外部工程统一使用。

### 2. 让 sample 侧接入

如果示例程序就是仓库里的 `examples/demo.cpp`，它的链接方式应该是：

```cmake
add_executable(demo examples/demo.cpp)
target_link_libraries(demo PRIVATE chlog::sink fmt::fmt)
```

这里的含义是：

- `demo` 只依赖公开接口，不直接关心 `src/sink.cpp` 是哪些文件。
- `fmt::fmt` 仍然由示例自己链接，因为示例里直接包含并调用了 `fmt`。

### 3. 让别的库或可执行程序使用

有两种常见接入方式。

#### 方式 A：源码方式，`add_subdirectory(...)`

适合同一个超级工程直接拉源码：

```cmake
add_subdirectory(third_party/chlog)
target_link_libraries(my_lib PRIVATE chlog::sink)
```

如果 `my_lib` 只是在自己的 `.cpp` 里使用 `Sink` / `ConsoleSink`，保持 `PRIVATE` 即可。

如果 `my_lib` 的头文件也暴露了 `Sink` / `ConsoleSink` 类型，就要改成：

```cmake
target_link_libraries(my_lib PUBLIC chlog::sink)
```

#### 方式 B：安装后用 `find_package(...)`

适合把 `chlog` 当独立三方库安装后复用。需要在 `CMakeLists.txt` 里补安装导出规则，并提供一个简单的 `cmake/chlogConfig.cmake.in`：

```cmake
@PACKAGE_INIT@
include("${CMAKE_CURRENT_LIST_DIR}/chlogTargets.cmake")
check_required_components(chlog)
```

安装命令示例：

```bash
cmake -S . -B build
cmake --build build
cmake --install build --prefix /your/prefix
```

消费侧这样写：

```cmake
find_package(chlog CONFIG REQUIRED)
target_link_libraries(my_lib PRIVATE chlog::sink)
```

如果 `chlog` 安装到了非系统目录，需要把安装前缀加入 `CMAKE_PREFIX_PATH`。

## 说明

上面这部分是“静态库接入方案备忘”，目的是记录当前项目后续的 CMake 组织方式和外部使用姿势。当前仓库如果仍然保持 `INTERFACE` 目标，那么 README 里的这一节应当视为推荐改法，而不是现状描述。
