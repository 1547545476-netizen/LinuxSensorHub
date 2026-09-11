# 参考项目和边界

本项目没有复制大型开源项目代码。参考项目只用于学习设计思想、工程结构和接口用法。

## Linux Kernel

链接：https://github.com/torvalds/linux

参考内容：

- 字符设备文件操作模型。
- `kfifo`、`wait_queue`、`poll`、`debugfs`、`sysfs` 的使用习惯。
- 驱动模块加载/卸载资源释放顺序。

本项目自己实现：

- `/dev/sensorhub0` 的数据结构、sample 协议、read/write/ioctl/poll。
- 虚拟传感器定时数据生成逻辑。
- sysfs/debugfs 输出字段。

## Linux Kernel Labs

链接：https://linux-kernel-labs.github.io/refs/heads/master/labs/device_drivers.html

参考内容：

- 字符设备驱动教学路径。
- 初学者理解 `file_operations`、等待队列、ioctl 的方式。

本项目自己实现：

- 完整工程目录、用户态服务、测试、脚本和面试文档。

## libevent

链接：https://github.com/libevent/libevent

参考内容：

- 事件驱动和 Reactor 的思想。
- 将 fd 事件注册到事件循环的抽象方式。

本项目自己实现：

- `EpollLoop`，直接基于 Linux `epoll_create1/epoll_ctl/epoll_wait`。

## muduo

链接：https://github.com/chenshuo/muduo

参考内容：

- Reactor + 线程模型的面试表达。
- 主循环负责 IO，worker 负责业务处理的设计思想。

本项目自己实现：

- 有界队列、worker 调度、日志落盘、TCP 广播。

## Fluent Bit

链接：https://github.com/fluent/fluent-bit

参考内容：

- 边缘设备日志/数据采集服务的产品形态。
- 配置、日志、采集、转发的整体思路。

本项目自己实现：

- 简化版配置解析、采集服务、UDP/TCP 导出和控制接口。

## can-utils

链接：https://github.com/linux-can/can-utils

参考内容：

- 嵌入式 Linux 用户态工具风格。
- 小工具围绕 Linux 设备接口做验证的方式。

本项目自己实现：

- `sensorctl`、`sensor_inject`、`sensor_subscribe`。

## 为什么不直接基于某个仓库改

求职项目最怕“能跑但讲不清楚”。本项目选择自己实现核心链路，是为了确保你能解释：

- 驱动怎么注册设备。
- `poll_wait` 为什么能唤醒 `epoll_wait`。
- 内核态为什么不能在定时器回调里睡眠。
- 用户态为什么要用 epoll 和 worker 线程。
- 队列满了为什么选择丢弃并计数。
- 日志轮转、控制面和 TCP 广播怎么实现。

