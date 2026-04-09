# chlog

## 编译

```bash
cmake -S . -B build -DCHLOG_BUILD_EXAMPLES=ON
cmake --build build -j
```

如果后续链接成功，运行命令是：

```bash
./build/demo
```

# 用法

```cmake
add_subdirectory(third_party/chlog)
target_link_libraries(my_lib PRIVATE chlog::sink)
```
