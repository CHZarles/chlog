# chlog 日志库设计

## 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                      Logger (单例)                         │
├─────────────────────────────────────────────────────────────┤
│  level() / headerPattern() / addRotatingFileSink()         │
│  stop()                                                     │
├─────────────────────────────────────────────────────────────┤
│                    ┌─────────────────┐                     │
│  chlog::info(...)  │  thread_local   │──┐                  │
│  chlog::warn(...)  │  queue cache    │  │                  │
│  chlog::error(...) │                 │  │                  │
│                    └─────────────────┘  │                  │
│                                         ▼                  │
│  ┌─────────────┐    ┌──────────────────────────────────┐   │
│  │ per-thread  │    │         SPSC Ring Queue          │   │
│  │ queue        │───▶│  (lock-free, pre-allocated)      │   │
│  └─────────────┘    └──────────────────────────────────┘   │
│         │                          │                        │
│         │                          ▼                        │
│         │                   ┌─────────────┐                  │
│         │                   │  worker     │                  │
│         │                   │  thread     │                  │
│         │                   └─────────────┘                  │
│         │                          │                        │
│         ▼                          ▼                        │
│  ┌─────────────┐           ┌────────────────┐              │
│  │ console     │           │ rotating file   │              │
│  │ sink        │           │ sink           │              │
│  └─────────────┘           └────────────────┘              │
└─────────────────────────────────────────────────────────────┘
```

## 核心设计决策

### 1. 为什么用 SPSC 队列

日志库的多线程场景：
- **生产者**：业务线程（多个）
- **消费者**：worker 线程（单个）

实际是 **MPSC**（多生产者、单消费者），但通过 per-thread queue 转换：

```
每线程一个 SPSC queue     每个 queue 被单一线程写入
         │                         │
         ▼                         ▼
    ┌──────────────────────────────────┐
    │     Logger::producerMap_         │
    │     (线程 → queue 映射)          │
    └──────────────────────────────────┘
                      │
                      ▼
              ┌───────────────┐
              │  worker 消费  │
              │  所有 queue   │
              └───────────────┘
```

### 2. 无锁热路径

每个线程维护一个 `thread_local` 队列指针缓存：

```cpp
// chlog.h
static thread_local std::shared_ptr<detail::ThreadQueue> t_queue_;

void Logger::log(Level lvl, std::string message, SourceLocation src) {
    auto queue = t_queue_;          // 读缓存（无锁）
    if (!queue) {                    // 首次调用
        std::lock_guard<std::mutex> lock(stateMutex_);
        queue = thisThreadQueueOwned();
        t_queue_ = queue;           // 写缓存（每线程一次）
    }
    // 后续调用完全无锁
}
```

**锁竞争分析**：
- 首次调用：获取一次锁
- 后续调用：0 次锁操作

### 3. 自动初始化

无需显式 `start()`：

```cpp
void Logger::log(...) {
    if (!running_.load(...)) {           // 检查运行状态
        std::lock_guard<std::mutex> lock(configMutex_);
        if (sinks_.empty()) {
            sinks_.push_back(make_unique<ConsoleSink>());  // 默认 console
            worker_ = std::thread(&Logger::workerLoop, this);
        }
        running_.store(true);
    }
}
```

### 4. Header-only HeaderFormatter

```cpp
// include/format.h
class HeaderFormatter {
public:
    explicit HeaderFormatter(std::string pattern = "{HMSf} [{level}] ")
        : pattern_(std::move(pattern)) {}

    std::string format(const LogEntry& entry) const {
        // 即时解析并格式化
        std::string result = pattern_;
        // 替换 {HMSf}, {level}, {date}, {thread}, {file}
        return result;
    }
};
```

设计原因：
- HeaderFormatter 无状态，轻量级
- 每次 log 都调用，放在热路径
- Header-only 避免额外编译单元

### 5. API 极简化

| 原 API | 新 API | 原因 |
|--------|--------|------|
| `chlog::instance()` | `Logger::instance()` | 消除命名空间冲突 |
| `addConsoleSink()` | 自动添加 | 99% 用 console |
| `start()` | 自动启动 | 无谓的显式调用 |
| `queueConfig()` | 隐藏 | 用户无需配置 |
| `chlog::instance().log(...)` | `chlog::info(...)` | 更简洁 |

## 使用方式

```cpp
#include "chlog.h"

// 方式 1：直接使用（自动初始化）
chlog::info("Hello {}", "world");

// 方式 2：配置后再使用
Logger::instance().level(Level::WARN);
Logger::instance().headerPattern("{date} [{level}] ");
Logger::instance().addRotatingFileSink("app.log", 10*1024*1024, 3);

// 方式 3：结合使用
Logger::instance().level(Level::DEBUG);
chlog::debug("Debug info");
chlog::info("User logged in");
```
