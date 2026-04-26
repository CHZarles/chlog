#include "queue.h"
#include <cstring>
#include <memory>

// 构造函数：预分配 capacity 个槽位，每个槽位大小为 sizeof(MsgHeader) +
// maxMessageSize。 使用 unique_ptr 管理各槽位内存，无需运行时频繁申请/释放。
// memset 0 初始化字节区，再用 placement new 构造
// MsgHeader，确保头结构被正确初始化。
RuntimeSPSCQueue::RuntimeSPSCQueue(size_t capacity, uint32_t maxMessageSize)
    : capacity_(capacity), maxMessageSize_(maxMessageSize), mask_(capacity - 1),
      slotBytes_(sizeof(MsgHeader) + maxMessageSize), slots_(capacity) {
  for (auto &slot : slots_) {
    slot = std::make_unique<std::byte[]>(slotBytes_);
    std::memset(slot.get(), 0, slotBytes_);
    new (slot.get()) MsgHeader();
  }
}
// slot：根据逻辑下标（producer_/consumer_）映射到实际环形槽位地址。
// 利用 mask_ = capacity_ - 1 将逻辑序号转为 0~capacity_-1 的环形下标。
// 当 capacity_ 为 2 的幂时，index & mask_ 等价于 index % capacity_，但更快。
MsgHeader *RuntimeSPSCQueue::slot(size_t index) {
  return reinterpret_cast<MsgHeader *>(slots_[index & mask_].get());
}

/* ---------------------------- 生产者部分 -----------------------------------
 */
// alloc：生产者申请可写槽位。
// 1. 检查 size 是否超限。
// 2. 用 acquire 读 consumer_（消费者位置），用 relaxed 读
// producer_（生产者位置）。
//    两者的差值即为队列当前长度，超过 capacity_ 说明队满。
// 3. 返回当前 producer_ 对应槽位的 MsgHeader 指针；此时生产者可向 payload
// 区写入数据。
// 4. 注意：仅返回指针，不推进 producer_，数据写完后需调用 push() 提交。
MsgHeader *RuntimeSPSCQueue::alloc(uint32_t size) {
  if (size > maxMessageSize_)
    return nullptr;

  const size_t consumer = consumer_.load(
      std::memory_order_acquire); // 确认下面发生的操作，不会优化到上面
  const size_t producer =
      producer_.load(std::memory_order_relaxed); // 只保证自己的原子性
  if ((producer - consumer) >= capacity_)        // 通过指针距离判断
    return nullptr;

  // 取出内存块
  MsgHeader *header = slot(producer);
  header->size = size;
  header->logId = 0;
  header->timestamp_us = 0;
  return header;
}

// push：提交刚刚 alloc 返回的槽位，推进生产者指针。
// 释放语义（release）确保 alloc 到 push 之间对 payload 的写入对消费者可见。
void RuntimeSPSCQueue::push() {
  producer_.fetch_add(1, std::memory_order_release);
}

/* -------------------------- 消费者部分------------------------------------*/
// front：消费者读取当前可消费槽位。
// 1. 用 acquire 读 producer_（生产者最新位置），用 relaxed 读 consumer_。
// 2. 若两者相等说明队列为空，返回 nullptr。
// 3. 否则返回 consumer_ 对应槽位的 MsgHeader 指针。
// 注意：只读不弹出，消费完成后需调用 pop() 弹出。
MsgHeader *RuntimeSPSCQueue::front() {
  const size_t producer = producer_.load(std::memory_order_acquire);
  const size_t consumer = consumer_.load(std::memory_order_relaxed);
  if (producer == consumer)
    return nullptr;
  return slot(consumer);
}

// pop：消费者消费完成后弹出当前槽位，推进消费者指针。
// 重置 header 字段（可选，但有助于区分"空槽"和"有效消息"）。
// 释放语义确保消费者对消息的处理结果对后续操作可见。
void RuntimeSPSCQueue::pop() {
  MsgHeader *header = slot(consumer_.load(std::memory_order_relaxed));
  header->size = 0;
  header->logId = 0;
  header->timestamp_us = 0;
  consumer_.fetch_add(1, std::memory_order_release);
}
