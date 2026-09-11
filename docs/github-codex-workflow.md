# GitHub 与 Codex：从修改到验证

## 先理解四个角色

| 名称 | 大白话 | 本项目里的用途 |
|---|---|---|
| 本地工程 | 电脑上的源码文件夹 | Codex 在这里读代码、改代码 |
| Git | 文件的版本记录工具 | 记录每次改了什么，可以比较和回退 |
| GitHub | 远程代码仓库与协作平台 | 保存已推送的版本、查看修改、运行 CI |
| Codex | 编程协作者 | 按需求改代码、解释源码、补测试、分析失败 |

保存文件不等于提交；提交不等于上传。`commit` 是本地存档，`push` 才是上传。它们不会自动把电脑上所有文件上传。

## 日常流程

```text
说清需求 -> Codex 读源码 -> feature 分支修改和测试
                              |
                              v
                        commit 本地存档
                              |
                              v
                        push 到 GitHub
                              |
                              v
                      PR：查看本次修改提案
                              |
                              v
                      Ubuntu CI 自动编译测试
                              |
                              v
                     人工检查通过后合并 main
```

`main` 是稳定版本；`feature/xxx` 是一个功能的工作分支；PR（Pull Request）是“请把这批改动合进去”的提案。绿色 CI 只代表配置的检查通过，不代表不存在任何问题。

在 Codex 中打开已经连接 GitHub 的干净仓库根目录，不要打开压缩包、上一级大文件夹或旧教学工程来推送。先让它执行 `git status`、`git remote -v`、`git log -1 --oneline` 确认目录、远端和版本。

本仓库的 [AGENTS.md](../AGENTS.md) 保存项目规则，包括中文教学、功能分支、测试和凭据保护。Codex 会在任务开始时读取适用的项目规则；新增或修改后，可新开一个任务让它先复述规则。规则不是密码，也不是强制的权限隔离。[OpenAI 官方说明](https://learn.chatgpt.com/docs/agent-configuration/agents-md)

## 可以直接对 Codex 说

**学源码：**

> 先不要改代码。读取 AGENTS.md，找到 EpollLoop::run 的实际位置，按文件和行号用 C 语言基础能理解的方式解释，每次讲一小段。

**做一个功能：**

> 先确认当前仓库和未提交修改。建立 feature/queue-tests 分支，为有界队列补充关闭后排空的测试。保留现有行为，给我解释改动，提交前检查敏感文件；能运行的测试实际运行，不能运行的说明原因。

**上传和检查：**

> 检查本次差异和敏感文件，只提交本次功能，推送功能分支并创建 PR。查看 GitHub Actions 的结果，失败就根据日志定位修复。先不要合并。

**同意合并：**

> 我已经看过这个 PR。请确认最新提交的 CI 通过，再合并到 main，并用快进方式同步本地 main。不要覆盖我未提交的修改。

## 自动测试如何看

打开仓库的 **Actions**，选择 **Linux CI**；也可以在 PR 的 **Checks** 查看。

- 黄色：排队或运行中；绿色：检查通过；红色：至少一步失败，需要打开对应步骤的日志。
- 推送 main、创建/更新面向 main 的 PR 时触发；单独推送未创建 PR 的功能分支不会触发，以减少重复运行。也可在 Actions 中手动 Run workflow。
- [linux-ci.yml](../.github/workflows/linux-ci.yml) 在 Ubuntu 24.04 上执行 Debug 构建、CTest、冒烟测试和 8 客户端模拟测试。
- 不加载内核驱动，不需要 OpenAI 密钥；驱动必须另在 Ubuntu 虚拟机实测。
- 工作流只读取仓库，不自动提交、不部署、不合并 PR。分支规则是协作约定，不代表已配置 GitHub 强制分支保护。
- Actions 使用 GitHub 的运行额度；本配置限制单次任务 10 分钟并取消同一分支过时的任务。未替你开通付费额度，具体额度/预算以账号的 Billing 设置为准。

## Windows 与 Ubuntu 怎样配合

Windows 可以编辑 Linux 源码，但 Windows 编译器不能验证 epoll 和内核模块。日常用 CI 做用户态自动检查，学习驱动时用自己的 Ubuntu 虚拟机。

Ubuntu 中旧教学目录和新 GitHub 仓库不是同一套历史。首次从 GitHub **克隆到一个新目录**，不要直接把旧目录连上远端后强制覆盖；私有仓库需要在 Ubuntu 单独完成 GitHub 官方登录。Windows 的登录不会自动复制过去。

以后在已克隆且没有未提交修改的 Ubuntu 仓库中运行：

```bash
git switch main
git pull --ff-only
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure --timeout 30
```

`pull --ff-only` 只允许简单快进同步；遇到本地分叉时停止，避免悄悄产生合并。不懂错误就交给 Codex 看，别用 `reset --hard` 或强制推送碰运气。

## 可选：在 GitHub 评论里叫 Codex 审查

这是另一层云端功能，不等于“GitHub 账号已经连接”，也不是本仓库 YAML 自动开启的。需要先为仓库设置 Codex Cloud，并在 Codex 的 Code review 设置中启用它；之后可在 PR 评论里写 `@codex review`。是否可用和用量以账号设置为准。[OpenAI 官方接入步骤](https://learn.chatgpt.com/docs/third-party/github)

先掌握本地 Codex + GitHub PR + CI 即可。不需要为了这个项目购买 Copilot，也不需要把 GPT 登录文件或 OpenAI 密钥放进仓库。

## 工具依赖与替代

| 工具 | 用途 | 许可证 | 替代方式 |
|---|---|---|---|
| GitHub CLI（gh） | 浏览器授权、建仓库、推送协作、查看 PR/CI | [MIT](https://github.com/cli/cli/blob/trunk/LICENSE) | Git + GitHub 网页 |
| actions/checkout v4.3.1 | CI 获取当前提交的源码 | [MIT](https://github.com/actions/checkout/blob/34e114876b0b11c390a56381ad16ebd13914f8d5/LICENSE) | 在已认证的受控环境用 Git 克隆并校验提交 |

它们是开发工具，不作为第三方源码复制进项目。checkout 固定完整提交号且不持久化凭据。程序本身仍依赖 Linux/POSIX、C++17 标准库和 CMake/Make。

## 保住账号安全

- 仓库只提交源码、测试和必要文档。不要上传聊天记录、`.codex/`、`auth.json`、`.env`、Cookie 或私钥。
- GitHub 网页授权在官方域名由账号本人确认；不要把密码、令牌或登录缓存发给别人。
- 私有仓库限制未获授权者访问，但不是绝对防泄漏。以后公开仓库，别人可以按许可证使用代码；仅下载不含凭据的项目，不会因此获得你的 GPT 登录权限。
- 发现真实秘密已经被提交，先撤销/轮换秘密，再处理 Git 历史；只删除文件或加入 .gitignore 不够。详见 [SECURITY.md](../SECURITY.md)。
