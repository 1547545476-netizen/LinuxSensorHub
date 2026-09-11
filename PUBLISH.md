# 上传 GitHub 前必读

## 只上传这一份干净工程

压缩包解压后有一个 `LinuxSensorHub` 文件夹。用于 GitHub 的是这个文件夹里的源码、文档、配置和构建文件；不要上传上级工作区，不要把旧 `.git` 或 `.bundle` 文件复制回来。

把 ZIP 本身上传到仓库只会得到一个压缩附件，不会自动变成可浏览的源码。请先解压，再上传内容或使用 Git 提交。

保留内容：核心 C/C++ 源码、UAPI、Makefile/CMake、测试、运行脚本、示例配置、架构、源码导读、参考资料和许可证。

没有包含：原 Git 历史与作者邮箱元数据、虚拟机镜像和截图、私人安装排障记录、私人学习与面试材料、登录配置、聊天导出、构建缓存及运行日志。原工程本地文件没有因此删除。

所有项目核心源码保持原样。README、忽略规则和本发布说明经过整理；本次没有宣称重新完成 Linux 编译、加载驱动或功能测试。

## 第一次发布

1. 登录自己的 GitHub，创建名为 `LinuxSensorHub` 的空仓库；担心公开范围时先选择 Private。
2. 如果准备使用下面的 Git 命令，不要让 GitHub 自动创建 README、许可证或 `.gitignore`，避免两个独立的初始提交。
3. 在 Windows 编辑器中打开这份解压后的工程，确认有 `README.md`、`.gitignore`、`.gitattributes`、`CMakeLists.txt` 和源码目录。注意保留点开头的文件。
4. 在该工程目录打开终端。下面全部命令只适用于这份没有旧历史的发布副本，不要在原学习仓库中照搬初始化步骤。

```bash
git init -b main
```

如果需要设置提交身份，在本仓库范围内设置显示名，并使用 GitHub 设置页提供的隐私邮箱。以下两行中的值必须自行替换，不要把账号密码填进去：

```bash
git config user.name "YOUR_DISPLAY_NAME"
git config user.email "YOUR_GITHUB_NOREPLY_EMAIL"
```

明确添加需要发布的目录和文件：

```bash
git add .gitignore .gitattributes CMakeLists.txt LICENSE COPYING README.md SECURITY.md PUBLISH.md config docs include kernel scripts tests userspace
git update-index --chmod=+x scripts/run_simulated.sh scripts/smoke_simulated.sh scripts/stress_simulated.sh scripts/verify_driver.sh tests/integration/simulated_daemon.sh
git diff --cached --name-only
git diff --cached
```

第一条查看文件清单，第二条查看实际内容；如果内容太长进入分页器，按 `q` 退出。确认没有令牌、密码、私人路径或不该公开的内容后，再提交：

```bash
git commit -m "Initial public source snapshot"
```

把下面的 `YOUR_GITHUB_NAME` 替换成自己的 GitHub 用户名；URL 里面不要嵌入 Token 或密码：

```bash
git remote add origin https://github.com/YOUR_GITHUB_NAME/LinuxSensorHub.git
git push -u origin main
```

需要 GitHub 登录时，通过 Git 提供的凭据管理器或浏览器正常登录。不要把任何服务的登录文件、Cookie 或密钥复制进仓库，也不需要向别人发送它们。

这份发布副本从新的初始提交开始，旧 main/feature 历史仍留在原学习工程，没有被伪造或删除。如果以后希望公开旧开发历史，需要单独审查后再决定如何发布，不要直接推送旧仓库的所有分支。

## 后续继续开发

在发布副本或从新仓库克隆的工程中创建功能分支：

```bash
git switch -c feature/next-module
```

每个独立功能修改后测试、检查差异并提交。合并回 main 后再发布。修改后的文件不自动继承这次发布包检查的结果，需要重新检查。

## GPT / Codex 账号安全

源码不是登录凭据。本工程不需要 GPT 服务，上传源码不需要提供 ChatGPT 密码、Codex 登录缓存或 OpenAI API 密钥。

特别不要上传 `.codex`、`auth.json`、`.env`、Cookie 导出、浏览器用户资料、SSH 私钥或带凭据的远程地址。详见 [SECURITY.md](SECURITY.md) 和其中的官方说明。

代码中出现技术名词、注释或 AI 工具名称，本身不会授予别人账号访问权；真正需要保护的是登录凭据和授权信息。反过来，删除这些名词也不能替代凭据检查。

本包整理采取白名单导出、内容模式检查和归档完整性校验，但不宣称绝对无风险。如怀疑凭据已在其他地方泄露，应先撤销或轮换，不能只删除本包文件。
