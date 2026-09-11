# 安全边界

## 发布源码不等于共享账号

本工程的源码不包含模型调用功能，构建和运行不需要 ChatGPT、Codex、OpenAI API、GitHub Token 或其他云端账号。

账号安全依赖保护凭据，而不是隐藏源码里的普通技术名词。不要向仓库、Issue、截图或聊天附件上传真实密码、登录 Cookie、访问令牌、API 密钥或私钥。

Codex 的登录缓存可能保存在 `~/.codex/auth.json` 或操作系统凭据存储中。文件形式的 `auth.json` 含有访问令牌，应当像密码一样保护，不能提交或分享。依据：[OpenAI 官方认证文档](https://learn.chatgpt.com/docs/auth)。

本包通过明确的文件清单导出，并排除旧 `.git`、个人编辑器配置、AI 工具状态、虚拟机截图、会话导出、虚拟机镜像和 Git bundle。没有读取或复制用户主目录中的实际认证文件。

`.gitignore` 已加入常见敏感文件规则，但它不是防泄露系统：已经被 Git 跟踪的文件不会因为新增忽略规则而自动消失，源码中直接写入的令牌也不会因此被阻止。

## 本项目运行边界

- TCP 广播当前绑定 `INADDR_ANY`，不是仅绑定回环地址。请在隔离虚拟机内测试，不要做公网端口转发或暴露给不可信局域网。
- TCP/UDP 传输没有应用层身份认证、访问控制或 TLS。示例不用于发送隐私、凭据或生产数据。
- 本地控制接口可以关闭服务和修改运行配置，应使用专用运行目录及适当的文件权限。
- 驱动会运行在内核空间。加载前保存虚拟机快照，不要直接用于主力机器或生产设备。
- systemd 单元仅作为示例；正式部署前必须明确运行用户、权限、绝对路径及限制措施。
- 本工程没有完成生产级安全审计。隐私检查通过不代表运行时没有漏洞。

## 如果以后误传凭据

1. 先在对应服务中撤销或轮换泄露的凭据，不能只依赖删除文件。
2. 检查账号登录和使用记录，并使用对应服务提供的安全功能。
3. 再处理 Git 历史、分支、发布附件、工作流日志、缓存和协作者副本。
4. 后续上传前检查暂存区；在可用时开启 GitHub secret scanning / push protection。

GitHub 官方说明强调先撤销或轮换泄露凭据；仅删除最新版本中的文件，不能保证旧历史和其他副本中的内容消失。依据：[GitHub 敏感数据处理指南](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/removing-sensitive-data-from-a-repository)。

## 源码可见性

公开 GitHub 仓库后，其他人可以看到代码并创建 fork；不能靠压缩、改名或忽略规则阻止别人取得公开源码。如果暂时不想公开源码，先建立 Private 仓库。本包没有修改原工程许可证，不承诺防复制或防冒名。依据：[GitHub 仓库可见性](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/setting-repository-visibility)。

本次检查仅针对所交付发布包，不代表整台电脑、旧仓库全部历史或既有账号已经接受审计，也不保证未来修改后的包仍然具有相同检查结果。
