# 源码导读

这份文档按“完全初学者”的顺序读，不要求你一开始就懂内核。

## 第 1 步：先看公共协议

文件：`include/sensorhub_uapi.h`

这里定义了内核和用户态都认识的数据结构：

- `sensorhub_sample`：一条传感器数据。
- `sensorhub_config`：采样周期和开关。
- `sensorhub_stats`：驱动统计信息。
- `SENSORHUB_IOC_*`：`ioctl` 命令号。

你要记住一句话：

```text
UAPI 是内核和用户态之间的合同。
```

## 第 2 步：看驱动入口

文件：`kernel/sensorhub_drv.c`

先找：

```c
module_init(sensorhub_init);
module_exit(sensorhub_exit);
```

这两个宏告诉内核：

- 加载模块时调用 `sensorhub_init`。
- 卸载模块时调用 `sensorhub_exit`。

然后看：

```c
static const struct file_operations sensorhub_fops
```

这是字符设备的核心。用户态调用 `open/read/write/ioctl/poll`，最后都会进入这里对应的函数。

## 第 3 步：理解数据怎么产生

驱动里用 `hrtimer` 定时产生数据。

流程是：

```text
sensorhub_timer_cb
  -> 构造 sensorhub_sample
  -> sensorhub_push_sample
  -> 写入 kfifo
  -> wake_up_interruptible
```

这里要重点理解：

- `hrtimer`：内核定时器。
- `kfifo`：内核环形缓冲。
- `wait_queue`：等待队列，用于阻塞读和 epoll 唤醒。

## 第 4 步：理解用户态服务

文件：`userspace/src/main.cpp`

主程序做了几件事：

1. 读取配置文件。
2. 创建 UDP socket。
3. 创建 TCP 广播器。
4. 创建 worker 线程。
5. 打开真实驱动或模拟设备。
6. 创建 `signalfd`。
7. 创建 Unix Domain Socket 控制接口。
8. 把这些 fd 注册到 epoll。
9. 进入事件循环。

这就是 Linux 服务程序的基本形态。

## 第 5 步：理解 epoll 封装

文件：`userspace/src/reactor/epoll_loop.cpp`

核心 API：

```cpp
epoll_create1
epoll_ctl
epoll_wait
```

你可以这样理解：

```text
epoll_create1 创建一个事件管理器。
epoll_ctl 把 fd 放进去。
epoll_wait 等待 fd 发生事件。
```

本项目不是为了写一个复杂框架，只是把 epoll 包成 `EpollLoop`，让主程序更清楚。

## 第 6 步：理解 worker 线程

文件：`userspace/include/bounded_queue.hpp`

主线程负责：

```text
从设备 fd 读数据 -> push 到队列
```

worker 线程负责：

```text
pop 队列 -> 写日志 -> UDP 发送 -> TCP 广播
```

为什么这样分？

因为主线程必须尽快回到 `epoll_wait`，不能被慢速磁盘或网络拖住。

## 第 7 步：理解控制命令

工具：`userspace/tools/sensorctl.cpp`

控制链路是：

```text
sensorctl -> Unix Domain Socket -> sensorhubd
```

支持命令：

- `stats`
- `reload`
- `shutdown`

这是很多 Linux daemon 的常见设计。

## 第 8 步：理解 TCP 订阅

文件：`userspace/src/net/tcp_broadcaster.cpp`

服务端监听一个 TCP 端口：

```text
sensorhubd listen 9090
sensor_subscribe connect 127.0.0.1:9090
```

有新 sample 时，worker 把文本行推给所有连接的客户端。

这部分可以在面试里展示 Socket 编程能力。

## 第 9 步：推荐调试顺序

先跑模拟模式：

```bash
bash scripts/smoke_simulated.sh
```

再看系统调用：

```bash
strace -f ./build/userspace/sensorctl stats -c build/smoke/sensorhub.conf
```

最后加载驱动：

```bash
sudo bash scripts/verify_driver.sh
```

## 第 10 步：你要能讲出的主线

面试时不要从“我写了很多文件”开始讲，而是讲数据流：

```text
内核定时器产生 sample，
写入 kfifo，
通过 wait_queue 唤醒 poll，
用户态 epoll 收到设备可读事件，
主线程批量读取后投递到有界队列，
worker 线程落盘并通过 UDP/TCP 导出，
控制工具通过 Unix Domain Socket 查询状态和关闭服务。
```

这条线讲顺了，项目就立起来了。

