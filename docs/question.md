## header only 有什么优缺点

- header only 的优点是:
  - 零成本配置, .cpp代码包含头文件即可用
  - 比起提供library，提供头文件不用担心平台兼容性问题
  - 日志库里用到了模板，header only的形式同时包含声明和实现，迎合模板实例化的需求
- header only 的缺点：
  - 没有办法闭源
  - 每次编译源码，都要重新编译模板库部分

## sink的构造函数为什么要写成protect

- 因为 Sink 是抽象基类，不希望被直接实例化。但是允许被子类实例化。

```c++
class Sink {
    protected:
    Sink() = default; // ← 只让子类调用，自己 new Sink() 会让编译报错
};
```

## 为什么 sink 的 write 接口用 std::string_view 类型

string_view 的语义是字符串的视图, 它本身不接管或者拷贝从函数外传递过来的任何字符串资源，所以这个函数传参的开销是很小的。
并且用string_view声明的形参支持接收的很多类型，比如 - 字符串字面量 - c 风格的常量字符指针 - string 类 - string 左值引用
这使得这个接口用起来很方便

## 这个接口在接受String引用的时候和接受字符串的时候有什么区别?

没有区别, 在运行过程中，本质都是会先根据 String 或者引用指向的 String 构造(隐式转换)一个String_view 再传给函数。 就像 write(std::string_view{s.data(), s.size()}, ...);

## 为什么没有区别?

从编译器的视角，一个字符串对象的引用，就是字符串原对象的别名，所以给函数传引用的本质就是给函数传原对象。

## 给一个函数参数声明为字符串引用的含义是什么? 形参没有对象语义吧

它的语义是在“函数作用域里一个绑定到实参对象的别名”

## 这是怎么实现的?

引用参数通常靠“偷偷传地址”实现，但语言语义把它包装成“对象别名”.

## inline 修饰函数的作用是什么

允许函数定义出现在头文件并被多个.cpp 是共享，不会产生重复定义错误。

## 写单例模式的要点是什么?

- 把构造函数变成私有函数
- 禁用拷贝函数和赋值函数
- 提供一个能获取 唯一实例 的函数

## 三五原则是什么

如果一个类需要自己管理资源，那么你通常不能只写一部分特殊成员函数，
必须成套考虑。

如果一个类显式定义了下面三者之一，通常也应该显式定义另外两个：

- 析构函数
- 拷贝构造
- 拷贝赋值

如果你需要自己定义这类资源管理行为，那么通常要一起考虑这五个：

- 析构函数
- 拷贝构造
- 拷贝赋值
- 移动构造
- 移动赋值

## 讲一个 atomic

## SPSC 环形队列中 capacity、mask、slotBytes、slots 如何配合实现无锁取模

环形队列有 N 个槽位，生产者/消费者各持有一个递增的逻辑序号 `producer_`/`comsumer_`。
要把它映射到 0～N-1 的槽位下标，用取模：

```
index = logical_counter % capacity
```

但取模运算（`%`）在热路径上较慢。当 `capacity` 是 2 的幂时，可以用位运算替代：

```
mask = capacity - 1
index = logical_counter & mask
```

原理：`logical_counter` 二进制低 M 位（`M = log2(capacity)`）恰好就是 `logical_counter % capacity`。

**各字段配合关系：**

```
slotBytes_ = sizeof(MsgHeader) + maxMessageSize_
```

每个槽位是一块连续的 `slotBytes_` 字节的预分配内存，包含固定头 + 最大 payload 缓冲区。

```
slots_ 存放 N 块独立内存，每块 slotBytes_ 字节
slot(i) = slots_[i % N]  // 环形下标
        = slots_[i & mask_]  // 用 mask 做 & 取模，比 % 更快
```

**为什么用 `std::unique_ptr<std::byte[]>` 而不是 `char`？**

`std::byte` 是 C++17 引入的类型，仅表示原始字节，无字符语义。用它强调这段内存是"未解释的二进制数据"，比 `char` 更准确，也避免误用字符串操作。
