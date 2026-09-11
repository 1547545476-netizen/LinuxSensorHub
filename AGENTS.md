# LinuxSensorHub 协作规则

## 项目与教学

- 先读 README.md、docs/architecture.md 和相关源码，再动手修改。
- 使用中文沟通。讲解面向有 C 基础、正在学习 Linux/C++ 的读者；引用实际文件和行号，解释新出现的类型、语法和系统调用。
- 核心逻辑的新增或变更需有简洁中文注释，重点说明资源所有权、线程同步和错误处理，不逐句翻译代码。
- 保持 C++17、CMake 和现有模块边界；不批量复制第三方项目，不做无关重构。
- 引入依赖时说明用途、许可证及替代方案。保留既有许可证与参考来源。

## 构建与验证

- 可以在 Windows 编辑，但 Linux 系统调用和内核模块必须在 Linux 验证，不能把 Windows 静态检查称为 Linux 实测。
- 用户态验证命令，从仓库根目录执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure --timeout 30
timeout --kill-after=5s 60s bash scripts/smoke_simulated.sh
CLIENTS=8 LINES=50 timeout --kill-after=5s 60s bash scripts/stress_simulated.sh
```

- 集成、冒烟、压力脚本顺序执行，避免已有实例占用端口。Debug 构建保留单元测试中的 assert。
- 修改功能要补相应测试；不得删除断言、隐藏失败或无限重试来使 CI 变绿。
- CI 验证的是用户态模拟模式，不是驱动加载。驱动修改应另在可恢复的 Ubuntu 虚拟机中编译、加载、测试和卸载；高权限操作前明确影响。
- 报告实际执行的命令、环境、结果与未验证项。不能虚构吞吐量、长稳时长、硬件实测或个人贡献。

## Git 工作方式

- 开始前查看 git status 和当前分支，保留用户未提交的修改。
- main 是稳定基线，功能开发用 feature/<主题>，修复用 fix/<主题>。一个独立功能一次清晰提交。
- 提交前检查 git diff、git diff --cached 和待提交文件清单；只暂存本次需要的文件，不使用 git add -f 绕过排除规则。
- 用户要求上传时推送功能分支并创建 PR，说明修改、测试和风险；得到用户同意且检查通过后再合并。
- 不强制推送、不重写旧历史、不自动切换仓库公开性、不购买服务或启用额外付费功能。
- README、架构图、使用说明随相关行为变化更新。旧教学副本和 Ubuntu 副本不会自动同步，操作前确认仓库路径与提交号。

## 安全边界

- 只操作工程所需文件。不要读取、复制、提交 Codex/GPT 登录缓存、Cookie、私钥、令牌、聊天记录或个人配置。
- 本项目不需要 OpenAI API 密钥，不在 GitHub Actions 中接入 API 计费的 Codex Action。
- GitHub 登录交给官方浏览器授权和系统凭据存储，不把令牌放进命令、URL、日志或仓库。
- .gitignore 和本文件不是安全沙箱；提交前仍需检查。发现疑似秘密时只报告文件位置，不复述秘密值。

## Code Review Rules

- 检查 fd 的关闭与回调生命周期，关注非阻塞 IO 的 EAGAIN/EINTR、短读短写及断开连接；不能把正常暂不可读当成协议完成。
- 检查队列关闭/排空、等待谓词和线程退出顺序，不能持有工作线程需要的锁再 join，不能破坏信号屏蔽的线程继承约束。
- TCP 当前无身份认证/TLS，默认监听所有 IPv4 网卡；禁止把它描述为可直接公网部署。模拟模式成功不能作为内核并发安全或硬件支持的证据。
