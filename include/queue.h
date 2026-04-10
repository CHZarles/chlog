#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// 每条队列消息的固定头部。
// 头后面紧挨着可变长 payload（线程名/文件/正文等打包数据）。
struct msgheader {
  // payload 的有效字节数（不包含 msgheader 自身）
  uint32_t size = 0;
  // 日志级别或日志类型 id（上层通常把 level 转成整数存这里）
  uint32_t logid = 0;
  // 生产时刻的时间戳（微秒）
  uint64_t timestamp_us = 0;
  // 指向“头后面的变长数据区”
  char *payload();
  const char *payload() const;
};

// runtimespscqueue: 运行时参数的单生产者/单消费者环形队列。
// 语义约束：
// 1) 只能有一个生产线程调用 alloc/push
// 2) 只能有一个消费线程调用 front/pop
// 3) 满队列/空队列时不阻塞，直接返回 nullptr 或不做事
class runtimespscqueue {
public:
  // capacity 通常要求是 2 的幂（便于用 mask_ 做快速取模）。
  // maxmessagesize 是单条消息 payload 的最大字节数。
  runtimespscqueue(size_t capacity, uint32_t maxmessagesize);
  // 为一条 size 字节的消息申请可写槽位。
  // 失败返回 nullptr（常见原因：size 超上限或队列已满）。
  msgheader *alloc(uint32_t size);

  // 提交刚刚 alloc 返回的槽位，推进生产指针。
  void push();
  // 读取当前可消费槽位；若队列为空返回 nullptr。
  msgheader *front();
  // 消费完成后弹出当前槽位，推进消费指针。
  void pop();

private:
  // 根据逻辑下标定位到实际环形槽位地址。
  msgheader *slot(size_t index);
  // 环形队列容量（槽位个数）
  size_t capacity_;
  // 单条消息允许的最大 payload 字节数
  uint32_t maxmessagesize_;
  // 容量为 2 的幂时用于快速取模：index & mask_
  size_t mask_;
  // 单个槽位占用总字节数（msgheader + payload 缓冲区）
  size_t slotbytes_;

  // 槽位内存池：每个元素是一块连续字节区，内部布局为 msgheader + payload
  std::vector<std::unique_ptr<std::byte[]>> slots_;

  // 生产位置计数器；64 字节对齐用于降低伪共享风险。
  alignas(64) std::atomic<size_t> producer_{0};
  alignas(64) std::atomic<size_t> comsumer_{0};
};
