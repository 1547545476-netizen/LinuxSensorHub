# 架构说明

## 项目目标

Linux SensorHub 模拟真实嵌入式 Linux 设备上的传感器数据链路：

```text
内核驱动产生数据 -> 用户态服务采集 -> 日志落盘 -> 控制命令查询状态
```

它不是为了做一个复杂产品，而是为了集中展示 Linux 驱动和 Linux C++ 系统编程能力。

## 总体架构图

```text
+------------------------------+
| kernel/sensorhub_drv.ko      |
|                              |
| hrtimer -> kfifo -> waitqueue|
|                  ^           |
|                  | ioctl     |
+------------------+-----------+
                   |
                   v
             /dev/sensorhub0
                   |
                   v
+------------------+------------------------------+
| userspace/sensorhubd                            |
|                                                 |
| EpollLoop                                       |
|  |-- sensor fd                                  |
|  |-- signalfd                                   |
|  |-- Unix Domain Socket                         |
|                                                 |
| BoundedQueue -> worker threads -> LogWriter     |
|                                -> UDP exporter  |
|                                -> TCP broadcast |
+-------------------------------------------------+
                   ^
                   |
          userspace/sensorctl
```

## 模块划分

| 模块 | 位置 | 作用 |
|---|---|---|
| UAPI | `include/sensorhub_uapi.h` | 内核和用户态共用的数据结构、ioctl 命令 |
| 驱动 | `kernel/sensorhub_drv.c` | 注册字符设备、生成虚拟传感器数据、支持 read/write/ioctl/poll |
| fd RAII | `userspace/include/fd.hpp` | 用 C++ 对文件描述符做自动关闭 |
| 配置 | `userspace/src/config` | 解析 `key=value` 配置 |
| epoll | `userspace/src/reactor` | 封装 `epoll_create1/epoll_ctl/epoll_wait` |
| 设备访问 | `userspace/src/device` | 读取真实 `/dev/sensorhub0` 或 timerfd 模拟数据 |
| 数据队列 | `userspace/include/bounded_queue.hpp` | 主线程和 worker 线程之间传递数据 |
| 存储 | `userspace/src/storage` | 把 sample 格式化成文本并做日志轮转 |
| TCP 广播 | `userspace/src/net` | 监听 TCP 端口，把实时 sample 推送给订阅客户端 |
| 控制面 | `userspace/src/main.cpp` | Unix Domain Socket 处理 stats/reload/shutdown |

## 数据流

1. 内核定时器周期性生成 `sensorhub_sample`。
2. 驱动把 sample 写入 `kfifo`。
3. 驱动唤醒 `wait_queue`。
4. 用户态 `epoll_wait` 发现设备 fd 可读。
5. 主线程批量读取 sample，放入有界队列。
6. worker 线程从队列取出 sample，写日志并通过 UDP 导出。
7. worker 线程把 sample 文本广播给所有 TCP 订阅客户端。
8. `sensorctl` 通过 Unix Domain Socket 查询状态或关闭服务。

## 为什么这样设计

- `miscdevice`：简化字符设备注册，把精力放在文件操作和同步机制上。
- `kfifo`：环形缓冲是驱动里很常见的数据结构。
- `wait_queue + poll`：这是驱动支持 `select/poll/epoll` 的关键。
- `epoll`：用户态服务能同时等设备、信号和控制 socket。
- `signalfd`：把信号变成 fd，统一纳入事件循环。
- `BoundedQueue`：队列有容量限制，能讨论背压和丢包策略。
- `sysfs/debugfs`：驱动运行时配置和调试状态，贴近真实驱动开发。

## 开发与验证链路

```text
本地 Codex + Git -> feature 分支 -> GitHub PR
                                      |
                                      v
                              Ubuntu 24.04 CI
                     Debug 构建 -> CTest -> 冒烟/多客户端
                                      |
                             人工审查后合并 main

独立验证：Ubuntu 虚拟机 -> 匹配内核头文件 -> 驱动编译/加载/测试/卸载
```

CI 不参与程序运行时数据链路，也不能代替内核驱动验证。规则与操作见 [GitHub + Codex 协作](github-codex-workflow.md)；流水线实现见 [linux-ci.yml](../.github/workflows/linux-ci.yml)。
