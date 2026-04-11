# RuntimeSPSCQueue 原理

## 什么是 SPSC

SPSC = Single Producer, Single Consumer（单生产者、单消费者）。

- 只能有一个线程负责写（生产）
- 只能有一个线程负责读（消费）
- 满队列/空队列时不阻塞，直接返回

这种约束使得**无需加锁**即可实现线程安全的消息传递。

## 为什么不需要锁

SPSC 队列的核心依赖：

1. **生产者只写 producer_，消费者只写 consumer_**——两者永不竞争同一个变量
2. **`atomic` 保证读写跨线程可见性**——不再需要 mutex
3. **预分配固定槽位**——不会触发动态内存分配（ allocator 竞争）

对于多生产者或多消费者，锁就成为必要条件，因为多个线程可能同时修改同一个指针。

## 逻辑计数器与环形映射

`producer_` 和 `consumer_` 是**无限递增的逻辑计数器**，初始值均为 0。

```
步骤        producer   consumer   差值    队列状态
初始化         0          0        0       空
生产3条       3          0        3       3条
消费2条       3          2        1       1条
再生产5条     8          2        6       6条
再生产2条    10          2        8       满
```

**差值 = 队列当前长度**，通过 `(producer - consumer) >= capacity_` 判断队满。

真正的环形映射只在 `slot(index)` 里发生：

```
slot(index) = slots_[index & mask_]
```

逻辑计数器的低 M 位（`M = log2(capacity)`）自动完成环形绕回。

## 为什么 capacity 必须是 2 的幂

当 `capacity = 2^M` 时，`index & (capacity - 1)` 等价于 `index % capacity`。

```
8 = 2^3, mask = 7 = 0b0111
index = 10 (0b1010)
index & mask = 0b1010 & 0b0111 = 0b0010 = 2  // 等价于 10 % 8
```

位运算 `&` 比取模 `%` 在 CPU 上更快，适合日志热路径。

## 两阶段提交：alloc 与 push

```
alloc() → 返回可写槽位指针，producer_ 不推进
push()  → 推进 producer_
```

分离是为了**保护生产者的写入窗口**：

```
1. alloc()      → 获得槽位
2. 写入 payload → 填充消息内容（此时 consumer 看不到）
3. push()       → 提交完成，consumer 可以消费
```

若 alloc 直接推进 `producer_`，消费者可能读到**写入了一半的消息**。

## 内存序（Memory Order）语义

```cpp
// alloc: 读 consumer 用 acquire，读 producer 用 relaxed
consumer_.load(std::memory_order_acquire);   // 确保看到消费者最新进度
producer_.load(std::memory_order_relaxed);   // 只是本地快照，允许稍旧值

// push: release 确保 alloc~push 之间的写入对消费者可见
producer_.fetch_add(1, std::memory_order_release);

// front: 读 producer 用 acquire，读 consumer 用 relaxed
producer_.load(std::memory_order_acquire);    // 确保看到生产者最新写入
consumer_.load(std::memory_order_relaxed);    // 只是本地快照

// pop: release 确保消费操作的结果可见
consumer_.fetch_add(1, std::memory_order_release);
```

## 槽位内存布局

```
每个槽位（slotBytes_ = sizeof(MsgHeader) + maxMessageSize_）：
+------------------+----------------------------------+
|     MsgHeader    |        payload 区                 |
|     (16 字节)    |     (maxMessageSize_ 字节)        |
+------------------+----------------------------------+
 ↑ this            ↑ this + 1 = payload() 返回这里
```

`payload()` 通过 `reinterpret_cast<char *>(this + 1)` 直接计算 payload 地址，无需额外存储指针。

## 预分配固定槽位

在构造函数里一次性把**所有内存都申请好**，程序运行期间**不再申请/释放**：

```cpp
RuntimeSPSCQueue(size_t capacity, uint32_t maxMessageSize)
    : ..., slots_(capacity) {
  for (auto &slot : slots_) {
    slot = std::make_unique<std::byte[]>(slotBytes_);
    std::memset(slot.get(), 0, slotBytes_);
    new (slot.get()) MsgHeader();
  }
}
```

**运行时流程对比：**

```
预分配（当前方案）：
  alloc() → 直接返回已有槽位指针（O(1)，无锁）
  pop()   → 只重置 header 字段，不释放内存

动态分配（按需）：
  alloc() → new 申请新内存
  pop()   → delete 释放内存
```

| | 预分配 | 动态分配 |
|---|---|---|
| 内存申请次数 | 仅初始化时一次 | 每条消息一次 |
| 生产消息开销 | O(1)，无锁 | O(1)，但可能触发分配器锁竞争 |
| 内存总占用 | 固定 = capacity × slotBytes | 动态增长 |

日志系统选预分配的原因：**高频写入时，动态分配会成为性能瓶颈**。每条消息都 `new`/`delete` 会触发分配器内部锁（尤其多线程场景），预分配完全绕过这个问题。

**代价：** 内存占用固定，无论实际使用多少，初始化时就占满。

## 伪共享与 64 字节对齐

`producer_` 和 `consumer_` 用 `alignas(64)` 对齐，确保它们落在**不同的 CPU 缓存行**。

同一缓存行的变量会被同时加载/刷到，频繁修改其中一个会导致另一个也失效（伪共享）。64 字节对齐使两个计数器互不干扰。

## 适用场景

- 日志异步写入（生产者：业务线程，消费者：IO 线程）
- 音视频帧缓冲
- 网络数据包队列
- 任何两线程间低延迟数据传递

不适用：多生产者/多消费者场景（需要 lock-free MPSC/MPMC 队列，复杂度更高）。