# chlog

一个轻量级、高性能 C++ 日志库，API 设计参考 spdlog。

## 特性

- **无锁热路径**：thread_local 队列缓存，极低锁竞争
- **自动初始化**：无需手动启动，首次调用即生效
- **异步写入**：后台 worker 线程批量消费队列
- **线程安全**：支持多线程并发日志
- **灵活配置**：日志级别、头部格式、文件轮转
- **Header-only**：仅需包含头文件即可使用

## 使用方式

### 快速开始（推荐）

```cpp
#include "chlog.h"

int main() {
    // 直接使用，自动初始化 console sink
    chlog::info("Hello, {}", "world");
    chlog::warn("Warning: {}", 42);
    chlog::error("Error occurred");

    // 程序结束前可选：优雅关闭
    Logger::instance().stop();
}
```

输出：
```
14:23:01.234567 [INFO]  Hello, world
14:23:01.234568 [WARN]  Warning: 42
14:23:01.234569 [ERROR]  Error occurred
```

### 完整配置示例

```cpp
#include "chlog.h"

int main() {
    // 1. 配置日志级别（DEBUG < INFO < WARN < ERROR < CRITICAL）
    Logger::instance().level(Level::INFO);

    // 2. 配置头部格式（可选，默认 "{HMSf} [{level}] "）
    Logger::instance().headerPattern("{date} {HMSf} [{level}] ");

    // 3. 添加文件输出（可选，可同时输出到 console 和 file）
    Logger::instance().addRotatingFileSink("app.log", 10 * 1024 * 1024, 3);

    // 4. 使用日志
    chlog::debug("Debug message");       // 不会输出（INFO 级别以上才输出）
    chlog::info("User {} logged in", username);
    chlog::warn("Connection slow: {}ms", latency);
    chlog::error("Failed to connect: {}", strerror(errno));

    // 5. 程序结束前优雅关闭
    Logger::instance().stop();
}
```

### 头部占位符

| 占位符 | 输出示例 | 说明 |
|--------|----------|------|
| `{date}` | `2024-01-15` | 日期 |
| `{HMSf}` | `14:23:01.234567` | 时间（微秒） |
| `{level}` | `INFO` | 日志级别 |
| `{thread}` | `worker-1` | 线程名 |
| `{file}` | `main.cpp:42` | 源文件和行号 |

## API 速查

### 全局函数（最常用）

```cpp
chlog::debug(fmt, args...);  // 调试级别
chlog::info(fmt, args...);   // 信息级别
chlog::warn(fmt, args...);   // 警告级别
chlog::error(fmt, args...);  // 错误级别
chlog::critical(fmt, args...); // 严重级别（始终输出）
```

### Logger 单例配置

```cpp
Logger::instance().level(Level::WARN);              // 设置日志级别
Logger::instance().headerPattern("{HMSf} [{level}] ");  // 设置头部格式
Logger::instance().addRotatingFileSink("app.log"); // 添加文件轮转
Logger::instance().stop();                         // 优雅关闭
```

## 编译

### 独立编译

```bash
cmake -S . -B build -DCHLOG_BUILD_EXAMPLES=ON
cmake --build build -j
./build/demo
```

### 集成到项目

```cmake
# CMakeLists.txt
add_subdirectory(chlog)
target_link_libraries(my_lib PRIVATE chlog::sink)
```

## 依赖

- C++17
- [fmt](https://github.com/fmtlib/fmt)（自动下载）