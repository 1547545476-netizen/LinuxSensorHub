# Linux SensorHub

基于 Linux 字符设备驱动和 C++17 的虚拟传感器采集工程。演示从内核生成采样，到用户态事件监听、线程间传递、日志写入和 TCP/UDP 导出的完整链路。

这是学习与技术展示工程，不是生产级数据采集平台。两种模式都使用合成数据，不依赖真实传感器，也不需要 GPT、云平台账号或 API 密钥。

## 模块与架构

```text
驱动模式：hrtimer -> kfifo -> /dev/sensorhub0 -> read
模拟模式：timerfd -> 到期次数 -> 用户态生成采样
                                      |
                                      v
                            主线程 epoll 事件循环
                                      |
                               BoundedQueue
                                      |
                                worker 线程
                         +------------+------------+
                         v            v            v
                      日志轮转      UDP 导出      TCP 广播

sensorctl -- Unix Domain Socket --> 主线程（stats/reload/shutdown）
sensor_subscribe -- TCP --> 订阅实时采样
```

| 模块 | 主要实现 |
|---|---|
| 字符设备驱动 | miscdevice、read/write/ioctl/poll、kfifo、spinlock、wait_queue、hrtimer、sysfs/debugfs |
| 用户态事件处理 | epoll、非阻塞 fd、timerfd、signalfd、Unix Domain Socket |
| 并发处理 | C++17 线程、互斥锁、条件变量、有界队列、关闭后排空 |
| 数据输出 | 文本日志与轮转、UDP 导出、TCP 客户端广播 |
| 工程管理 | CMake、Makefile、CTest、单元测试、模拟模式集成测试、systemd 示例 |

详细资料：[架构说明](docs/architecture.md)、[源码导读](docs/source-walkthrough.md)、[参考项目与边界](docs/references.md)、[GitHub + Codex 协作](docs/github-codex-workflow.md)。

## 构建与测试

在 Linux 中执行，建议先使用隔离的 Ubuntu 虚拟机。以下命令从项目根目录开始：

```bash
sudo apt update
sudo apt install -y build-essential cmake git
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure --timeout 30
```

CTest 包含配置/队列单元测试及模拟服务集成测试。集成测试会创建本地服务，使用 TCP 9090；请先停止手动运行的同端口实例，避免端口冲突。

控制连接另有 C++ 回归测试，覆盖延迟/分段命令、慢客户端不阻塞其他请求、短写、超长输入、半关闭和提前断开。控制协议为一条换行结尾的命令/连接，命令最长 127 字节，同时最多保留 64 个未完成控制连接。冒烟与多客户端脚本会检查 `shutdown` 回复及服务退出，之后才报告成功。

仓库包含 [Ubuntu 自动测试工作流](.github/workflows/linux-ci.yml)：面向 main 的 PR 及 main 推送会执行 Debug 构建、CTest、模拟冒烟和多客户端测试。实际结果以对应提交的 GitHub Actions 记录为准；工作流不验证驱动加载，也不调用 OpenAI API。

## 模拟模式演示

终端一，从项目根目录执行：

```bash
bash scripts/run_simulated.sh
```

脚本自动构建并生成 `build/simulated.conf`，服务在前台运行。终端二进入同一项目根目录后执行：

```bash
./build/userspace/sensorctl stats -c build/simulated.conf
./build/userspace/sensor_subscribe -n 5 -c build/simulated.conf
tail -n 5 build/logs/sensorhub.log
./build/userspace/sensorctl shutdown -c build/simulated.conf
```

预期可看到统计信息、以 `seq=` 开头的采样文本以及日志内容。`shutdown` 仅关闭本项目服务，不会关闭操作系统。模拟模式不加载内核模块，也不能代替驱动实测。

附加脚本应单独运行，不要与上面的服务同时占用端口：

```bash
bash scripts/smoke_simulated.sh
CLIENTS=8 LINES=50 bash scripts/stress_simulated.sh
```

## 内核驱动验证

仅在可恢复的 Linux 虚拟机中加载学习用模块。先保存虚拟机快照，并安装与运行内核匹配的头文件；Secure Boot 等设置也可能影响模块加载。

```bash
sudo apt install -y linux-headers-$(uname -r)
make -C kernel
sudo insmod kernel/sensorhub_drv.ko
ls -l /dev/sensorhub0
sudo dmesg | tail -n 20
```

`config/sensorhub.conf` 的默认值是 `simulate_device=false`，对应驱动模式。在设备访问权限满足时，从项目根目录执行：

```bash
./build/userspace/sensorhubd -c config/sensorhub.conf
```

另一个终端可通过同一配置调用 `sensorctl`。停止服务后卸载模块：

```bash
./build/userspace/sensorctl shutdown -c config/sensorhub.conf
sudo rmmod sensorhub_drv
```

`sudo bash scripts/verify_driver.sh` 主要检查模块加载、设备节点与 sysfs 状态，不等于驱动并发、长稳或全部读写路径测试。

## 目录

```text
config/       无凭据的示例配置与 systemd 单元
include/      内核和用户态共用的 UAPI
kernel/       内核模块源码及 Makefile
userspace/    C++ 服务、辅助工具及 CMake 构建文件
tests/        单元测试与集成测试
scripts/      模拟运行、冒烟、压力和驱动检查脚本
docs/         架构、源码导读与参考资料
.github/      Ubuntu CI 与 PR 检查模板
AGENTS.md     Codex 项目协作与教学规则
```

## 安全与发布边界

- 默认 TCP 服务绑定所有 IPv4 网卡，没有身份认证或 TLS。只在本机/隔离网络验证，不要做公网端口映射；详见 [SECURITY.md](SECURITY.md)。
- `config/sensorhub.service` 是部署模板，不是已经配置完权限、工作目录和沙箱的生产服务。
- 没有引入第三方源码库；依赖 Linux/POSIX、C++17 标准库和相应构建工具。
- 源码包含测试不等于已经在所有 Ubuntu/内核版本通过。本发布包的整理与隐私检查不构成新增的运行测试结果，也不包含性能或稳定性保证。
- 首次上传步骤和凭据保护说明见 [PUBLISH.md](PUBLISH.md)。发布快照不包含旧 Git 历史或私人学习资料。

## 许可证

保留原工程的 [LICENSE](LICENSE) 声明，并附上 GNU GPL version 2 正文 [COPYING](COPYING)。参考资料及本项目的实现边界见 [docs/references.md](docs/references.md)。本次整理不改变既有许可声明，也没有添加任何账号授权。
