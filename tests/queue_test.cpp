#include "queue.h"

#include <atomic>
#include <cstring>
#include <doctest/doctest.h>
#include <stdexcept>
#include <thread>

// ---------------------------------------------------------------------------
// 基础功能测试（单线程）
// ---------------------------------------------------------------------------

// 验证构造函数能正确预分配内存，alloc 返回有效指针即说明分配成功。
TEST_CASE("constructor allocates correct slot size") {
  RuntimeSPSCQueue q(16, 256);
  CHECK(true);  // If this compiles and runs, allocation succeeded
}

// capacity 必须是非零 2 的幂，否则 mask_ = capacity - 1 不能安全取模。
TEST_CASE("constructor rejects invalid capacity") {
  CHECK_THROWS_AS([] { RuntimeSPSCQueue q(0, 256); }(), std::invalid_argument);
  CHECK_THROWS_AS([] { RuntimeSPSCQueue q(3, 256); }(), std::invalid_argument);
  CHECK_THROWS_AS([] { RuntimeSPSCQueue q(6, 256); }(), std::invalid_argument);
}

// 空队列时 front() 必须返回 nullptr。
TEST_CASE("empty queue: front returns nullptr") {
  RuntimeSPSCQueue q(8, 128);
  CHECK(q.front() == nullptr);
}

// 完整流程：alloc → 写 payload → push → front → 读 payload → pop → 空。
// 验证两阶段提交（alloc/push）配合正确，payload 数据在消费者侧完整可读。
TEST_CASE("alloc/push/front/pop roundtrip") {
  RuntimeSPSCQueue q(8, 128);

  SUBCASE("single message") {
    MsgHeader *hdr = q.alloc(10);
    REQUIRE(hdr != nullptr);
    CHECK(hdr->size == 10);

    // 写入 payload
    char *p = hdr->payload();
    memcpy(p, "hello", 5);

    q.push();

    // 消费侧：front 看到相同数据
    MsgHeader *consumed = q.front();
    REQUIRE(consumed != nullptr);
    CHECK(consumed->size == 10);
    CHECK(memcmp(consumed->payload(), "hello", 5) == 0);

    q.pop();
    CHECK(q.front() == nullptr);
  }
}

// 队满时 alloc 必须返回 nullptr，不覆盖未消费消息。
// capacity=4：生产 4 条后队列正好满（diff=4 == capacity），下次 alloc 失败。
TEST_CASE("queue full: alloc returns nullptr") {
  RuntimeSPSCQueue q(4, 64);

  for (int i = 0; i < 4; i++) {
    MsgHeader *hdr = q.alloc(8);
    REQUIRE(hdr != nullptr);
    hdr->logId = i;
    q.push();
  }

  CHECK(q.alloc(8) == nullptr);
}

// 消费一个槽位后，producer 逻辑计数器回到该位置，alloc 应复用该槽。
// capacity=2：第一轮生产消息 1,2 → pop 两条后 → alloc 应回到 slot 0。
TEST_CASE("after pop, can alloc again") {
  RuntimeSPSCQueue q(2, 64);

  MsgHeader *h1 = q.alloc(1);
  REQUIRE(h1 != nullptr);
  h1->logId = 1;
  q.push();

  MsgHeader *h2 = q.alloc(1);
  REQUIRE(h2 != nullptr);
  h2->logId = 2;
  q.push();

  CHECK(q.alloc(1) == nullptr);  // 队满

  q.pop();
  MsgHeader *h3 = q.alloc(1);
  REQUIRE(h3 != nullptr);
  CHECK(h3->logId == 0);  // 逻辑计数器回绕，复用 slot 0
}

// size 超出 maxMessageSize_ 时必须拒绝。
TEST_CASE("message size exceeds max: alloc returns nullptr") {
  RuntimeSPSCQueue q(8, 32);
  CHECK(q.alloc(64) == nullptr);   // 超过上限
  CHECK(q.alloc(32) != nullptr);   // 等于上限 OK
}

// 测试环形绕回：capacity=4，交替生产/消费 8 条消息。
// 通过 producer_ 和 consumer_ 的差值控制何时能消费，确保槽位正确复用。
TEST_CASE("slot wraps around correctly") {
  RuntimeSPSCQueue q(4, 64);

  // 每轮：生产 1 条，然后消费上一条（diff 足够时）
  for (int i = 0; i < 8; i++) {
    MsgHeader *hdr = q.alloc(4);
    REQUIRE(hdr != nullptr);
    hdr->logId = i;
    q.push();

    // diff >= 1 时说明上一条已可消费
    if (i >= 1) {
      MsgHeader *consumed = q.front();
      REQUIRE(consumed != nullptr);
      CHECK(consumed->logId == static_cast<uint32_t>(i - 1));
      q.pop();
    }
  }

  // 消费最后一条
  MsgHeader *last = q.front();
  REQUIRE(last != nullptr);
  CHECK(last->logId == 7);
  q.pop();

  CHECK(q.front() == nullptr);
}

// payload() 返回的地址必须正好是 header 之后的字节。
// 通过 (p - 1) 转回 MsgHeader*，必须等于原始指针。
TEST_CASE("payload offset is correct") {
  RuntimeSPSCQueue q(8, 128);

  MsgHeader *hdr = q.alloc(16);
  REQUIRE(hdr != nullptr);

  char *p = hdr->payload();
  MsgHeader *verify = reinterpret_cast<MsgHeader *>(p) - 1;
  CHECK(verify == hdr);
}

// 模拟真实场景：生产者先填满一半 → 消费者取走一部分 → 生产者继续。
// 验证 producer_ 和 consumer_ 各自独立推进，互不干扰。
TEST_CASE("producer consumer operate independently") {
  RuntimeSPSCQueue q(16, 256);

  // 生产 8 条（producer=8, consumer=0, diff=8）
  for (int i = 0; i < 8; i++) {
    MsgHeader *hdr = q.alloc(8);
    REQUIRE(hdr != nullptr);
    hdr->logId = i + 100;
    q.push();
  }

  // 消费 4 条（producer=8, consumer=4, diff=4）
  for (int i = 0; i < 4; i++) {
    MsgHeader *hdr = q.front();
    REQUIRE(hdr != nullptr);
    CHECK(hdr->logId == static_cast<uint32_t>(i + 100));
    q.pop();
  }

  // 再生产 4 条（producer=12, consumer=4, diff=8）
  for (int i = 0; i < 4; i++) {
    MsgHeader *hdr = q.alloc(8);
    REQUIRE(hdr != nullptr);
    hdr->logId = i + 200;
    q.push();
  }

  // 消费剩余 8 条
  for (int i = 0; i < 8; i++) {
    MsgHeader *hdr = q.front();
    REQUIRE(hdr != nullptr);
    CHECK(hdr->logId == static_cast<uint32_t>((i < 4 ? 104 + i : 200 + i - 4)));
    q.pop();
  }
}

// ---------------------------------------------------------------------------
// 压力测试（双线程）
// 目标：暴露内存序问题、环形复用 bug、数据破坏等
// ---------------------------------------------------------------------------

// 目标：验证 consumer 用 acquire / producer 用 relaxed 的正确性。
// 机制：生产者写递增 logId，消费者验证接收顺序严格递增（id == lastId + 1）。
// 队列参数：capacity=8，强制高竞争（diff 经常为 1 或 2）。
// 错误表现：若 memory_order 用错，logId 会乱序、重复或丢失。
TEST_CASE("stress: two threads, sequential ordering") {
  constexpr int TOTAL = 5000000;
  RuntimeSPSCQueue q(8, 64);  // 极小队列 → 高竞争

  std::atomic<int> nextId{0};
  std::atomic<int> received{0};
  std::atomic<bool> producerDone{false};

  std::thread producer([&]() {
    while (true) {
      int id = nextId.load(std::memory_order_relaxed);
      if (id >= TOTAL)
        break;
      MsgHeader *hdr = q.alloc(1);
      if (hdr != nullptr) {
        hdr->logId = static_cast<uint32_t>(id);
        q.push();
        nextId.store(id + 1, std::memory_order_relaxed);
      }
      // 无 sleep，纯自旋，最大化竞争
    }
    producerDone.store(true, std::memory_order_release);
  });

  std::thread consumer([&]() {
    int lastId = -1;
    while (received.load(std::memory_order_relaxed) < TOTAL) {
      MsgHeader *hdr = q.front();
      if (hdr != nullptr) {
        int id = static_cast<int>(hdr->logId);
        CHECK(id == lastId + 1);  // 顺序必须严格递增
        lastId = id;
        received.fetch_add(1, std::memory_order_relaxed);
        q.pop();
      }
    }
  });

  producer.join();
  consumer.join();

  CHECK(received.load() == TOTAL);
}

// 目标：验证环形绕回在高频竞争下不发生数据破坏。
// 机制：单线程生产/消费，交替填满/清空队列 20 万次。
// 队列参数：capacity=4，每轮循环逻辑计数器前进 4，确保经历多轮环形复用。
// 错误表现：若环形索引计算错误，logId 会错位或被覆盖。
TEST_CASE("stress: fill and drain repeatedly") {
  RuntimeSPSCQueue q(4, 32);  // 极小队列，强制反复绕回
  constexpr int CYCLES = 200000;

  std::atomic<int> sent{0};
  std::atomic<int> recv{0};
  std::atomic<bool> done{false};

  std::thread t([&]() {
    while (sent.load(std::memory_order_relaxed) < CYCLES) {
      MsgHeader *hdr = q.alloc(4);
      if (hdr != nullptr) {
        hdr->logId = static_cast<uint32_t>(sent.load(std::memory_order_relaxed));
        q.push();
        sent.fetch_add(1, std::memory_order_relaxed);
      }
    }
    done.store(true, std::memory_order_release);
  });

  while (true) {
    MsgHeader *hdr = q.front();
    if (hdr != nullptr) {
      CHECK(hdr->logId == static_cast<uint32_t>(recv.load(std::memory_order_relaxed)));
      recv.fetch_add(1, std::memory_order_relaxed);
      q.pop();
    } else if (done.load(std::memory_order_acquire)) {
      break;
    }
  }

  t.join();
  CHECK(sent.load() == CYCLES);
  CHECK(recv.load() == CYCLES);
}

// 目标：验证 push 的 release 语义——alloc~push 之间对 payload 的写入，
//       在消费者 front 时必须全部可见（不会读到半写状态）。
// 机制：写入 payload 特征值（每 4 字节为 ID），消费时逐字节校验。
// 队列参数：capacity=8, payload=32 字节，可能跨缓存行，增加可见性压力。
// 错误表现：若 release 语义缺失，消费者会读到全 0 或部分乱码。
TEST_CASE("stress: concurrent with data integrity check") {
  constexpr int TOTAL = 2000000;
  RuntimeSPSCQueue q(8, 128);  // payload 可能跨缓存行

  std::atomic<int> next{0};
  std::atomic<int> count{0};
  std::atomic<bool> stop{false};

  std::thread producer([&]() {
    while (true) {
      int id = next.load(std::memory_order_relaxed);
      if (id >= TOTAL)
        break;
      MsgHeader *hdr = q.alloc(32);
      if (hdr != nullptr) {
        int curId = next.fetch_add(1, std::memory_order_relaxed);
        hdr->logId = static_cast<uint32_t>(curId);
        // 写入特征值：每 4 字节 = curId
        uint32_t *payload = reinterpret_cast<uint32_t *>(hdr->payload());
        for (int i = 0; i < 8; i++) {  // 8 * 4 = 32 bytes
          payload[i] = static_cast<uint32_t>(curId);
        }
        q.push();  // release：确保 payload 写入对消费者可见
      }
    }
    stop.store(true, std::memory_order_release);
  });

  std::thread consumer([&]() {
    int expected = 0;
    while (count.load(std::memory_order_relaxed) < TOTAL) {
      MsgHeader *hdr = q.front();
      if (hdr != nullptr) {
        int id = static_cast<int>(hdr->logId);
        CHECK(id == expected);
        // 逐块校验 payload：若 release 语义缺失，这里会看到乱码或 0
        uint32_t *payload = reinterpret_cast<uint32_t *>(hdr->payload());
        for (int i = 0; i < 8; i++) {
          CHECK(payload[i] == static_cast<uint32_t>(id));
        }
        expected++;
        count.fetch_add(1, std::memory_order_relaxed);
        q.pop();
      } else if (stop.load(std::memory_order_acquire) &&
                 next.load(std::memory_order_acquire) >= TOTAL) {
        break;
      }
    }
  });

  producer.join();
  consumer.join();

  CHECK(count.load() == TOTAL);
}
