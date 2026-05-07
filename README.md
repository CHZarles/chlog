# chlog

一个轻量级、高性能 C++ 日志库，API 设计参考 spdlog。

## 特性

- **无锁热路径**：thread_local 队列缓存，极低锁竞争
- **自动初始化**：无需手动启动，首次调用即生效
- **异步写入**：后台 worker 线程批量消费队列
- **线程安全**：支持多线程并发日志
- **灵活配置**：日志级别、头部格式、文件轮转
- **Header-only**：仅需包含头文件即可使用

## 快速开始

```cpp
#include "chlog.h"

int main() {
    // 自动初始化，直接使用
    chlog::info("Hello, {}", "world");
    chlog::warn("Warning: {}", 42);
    chlog::error("Error occurred");

    // 优雅关闭（可选）
    Logger::instance().stop();
}
```

输出：
```
14:23:01.234567 [INFO]  Hello, world
14:23:01.234568 [WARN]  Warning: 42
14:23:01.234569 [ERROR]  Error occurred
```

## 配置

### 日志级别

```cpp
Logger::instance().level(Level::WARN);  // 只输出 WARN 及以上
```

日志级别（从低到高）：`DEBUG` < `INFO` < `WARN` < `ERROR` < `CRITICAL`

### 头部格式

```cpp
Logger::instance().headerPattern("{date} {HMSf} [{level}] [{thread}] {file} ");
```

可用占位符：
- `{date}` — 日期 `2024-01-15`
- `{HMSf}` — 时间 `14:23:01.234567`
- `{level}` — 日志级别
- `{thread}` — 线程名
- `{file}` — 源文件和行号

### 文件输出

```cpp
Logger::instance().addRotatingFileSink("app.log", 10 * 1024 * 1024, 3);
```

- 单文件最大 10MB
- 保留最多 3 个备份（`app.log.1`, `app.log.2`, `app.log.3`）

## API 参考

### 全局函数

| 函数 | 说明 |
|------|------|
| `chlog::debug(fmt, args...)` | 调试级别 |
| `chlog::info(fmt, args...)` | 信息级别 |
| `chlog::warn(fmt, args...)` | 警告级别 |
| `chlog::error(fmt, args...)` | 错误级别 |
| `chlog::critical(fmt, args...)` | 严重级别 |

支持 `fmt::format` 格式化语法：
```cpp
chlog::info("User {} logged in from {}", username, ip);
chlog::error("Connection failed: errno={}", errno);
```

### Logger 单例

```cpp
Logger::instance().level(Level);              // 设置日志级别
Logger::instance().headerPattern(pattern);    // 设置头部格式
Logger::instance().addRotatingFileSink(...);  // 添加文件 sink
Logger::instance().stop();                    // 优雅关闭
```

## 编译

```bash
cmake -S . -B build -DCHLOG_BUILD_EXAMPLES=ON
cmake --build build -j
./build/demo
```

### 集成到项目

```cmake
add_subdirectory(chlog)
target_link_libraries(my_lib PRIVATE chlog::sink)
```

## 依赖

- C++17
- [fmt](https://github.com/fmtlib/fmt)（自动下载）