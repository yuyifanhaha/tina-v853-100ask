# Git 仓库管理文档 — tina-v853-100ask

## 仓库地址

- **GitHub**: `git@github.com:yuyifanhaha/tina-v853-100ask.git`
- **本地路径**: `/home/yyf/tina/06_Tina_SDK包/tina-v853-open/`

---

## 1. 仓库管理策略

### 1.1 核心原则

Tina SDK 是一个**完整的 SDK 项目**（约 26GB），但 Git 仓库只应跟踪**源代码、配置文件和文档**，不跟踪构建产物和预编译工具链。

```
Git 跟踪的内容:
  ✅ 应用层源码 (openwrt/package/ 下的自定义包)
  ✅ 设备配置文件 (device/config/)
  ✅ 内核驱动修改 (kernel/linux-4.9/drivers/ 下的修改)
  ✅ 构建脚本 (build/)
  ✅ 项目文档 (PROJECT_PLAN.md, README.md)
  ✅ 补丁文件 (.patch)
  ✅ .gitignore

Git 不跟踪的内容 (在 .gitignore 中排除):
  ❌ 编译输出 (out/)
  ❌ 预编译工具链 (prebuilt/)
  ❌ .repo/ (SDK 管理工具)
  ❌ 内核编译产物 (kernel/*.o, *.ko, Image, zImage)
  ❌ OpenWrt/Buildroot 构建产物 (staging_dir, build_dir, dl, feeds)
  ❌ 临时文件和日志
```

### 1.2 分支策略

```
main ──────────────────────────────────────────────→
  │
  ├── feature/vi-module        # 视频输入模块开发
  ├── feature/vo-module        # 视频输出模块开发
  ├── feature/venc-module      # 视频编码模块开发
  ├── feature/audio-module     # 音频模块开发
  ├── feature/rtsp-server      # RTSP 服务器开发
  ├── feature/mp4-muxer        # MP4 封装开发
  ├── feature/mp4-demuxer      # MP4 解封装开发
  └── feature/integration      # 系统集成
```

### 1.3 Tag 策略

- **v0.1.0** — SDK 编译通过 (已完成)
- **v0.2.0** — 摄像头预览 + 显示
- **v0.3.0** — H.264/H.265 硬件编解码
- **v0.4.0** — 音频采集播放 + 编解码
- **v0.5.0** — RTSP 流媒体传输
- **v0.6.0** — MP4 封装与解封装
- **v1.0.0** — 完整系统集成与验证

---

## 2. 初始化设置

### 2.1 本地 Git 配置

```bash
# 在 SDK 根目录
cd /home/yyf/tina/06_Tina_SDK包/tina-v853-open/

# 初始化仓库
git init

# 配置用户信息
git config user.name "yuyifanhaha"
git config user.email "yuyifanhaha@gmail.com"

# 关联远程仓库
git remote add origin git@github.com:yuyifanhaha/tina-v853-100ask.git
```

### 2.2 SSH Key 配置 (如未配置)

```bash
# 生成 SSH Key
ssh-keygen -t ed25519 -C "yuyifanhaha@gmail.com"

# 添加到 ssh-agent
eval "$(ssh-agent -s)"
ssh-add ~/.ssh/id_ed25519

# 复制公钥到 GitHub
cat ~/.ssh/id_ed25519.pub
# → 在 GitHub Settings → SSH and GPG keys → New SSH Key
```

---

## 3. 日常工作流

### 3.1 典型开发流程

```bash
# 1. 更新主分支
git checkout main
git pull origin main

# 2. 创建功能分支
git checkout -b feature/vi-module

# 3. 开发、修改代码
# ... 编写代码 ...

# 4. 查看变更
git status
git diff

# 5. 暂存变更
git add <files>

# 6. 提交
git commit -m "feat(vi): add VI module with ISP color tuning

- Implement VI camera capture pipeline
- ISP parameters tuned for 20% color improvement
- Support NV21 pixel format at 1080p30

Co-Authored-By: Claude <noreply@anthropic.com>"

# 7. 推送到远程
git push origin feature/vi-module

# 8. 在 GitHub 上创建 Pull Request
# 9. Code Review 后合并到 main
```

### 3.2 Commit Message 规范

采用 [Conventional Commits](https://www.conventionalcommits.org/) 格式：

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Type 类型**:

| Type | 说明 |
|------|------|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `docs` | 文档更新 |
| `style` | 代码格式 (不影响功能) |
| `refactor` | 重构 (非新增/修复) |
| `perf` | 性能优化 |
| `test` | 测试相关 |
| `chore` | 构建/工具/依赖 |
| `revert` | 回退 |

**Scope 范围**:

| Scope | 对应模块 |
|-------|----------|
| `vi` | 视频输入 (VI) |
| `vo` | 视频输出 (VO) |
| `venc` | 视频编码 (VENC) |
| `vdec` | 视频解码 (VDEC) |
| `ai` | 音频输入 (AI) |
| `ao` | 音频输出 (AO) |
| `aenc` | 音频编码 (AENC) |
| `adec` | 音频解码 (ADEC) |
| `rtsp` | RTSP 流媒体 |
| `muxer` | MP4 封装 |
| `demuxer` | MP4 解封装 |
| `isp` | ISP 图像处理 |
| `g2d` | 2D 图形加速 |
| `kernel` | 内核驱动 |
| `config` | 设备配置 |
| `sdk` | SDK 构建系统 |
| `docs` | 文档 |

**示例**:

```bash
git commit -m "feat(vi): implement VI camera capture with 1080p30

- Configure VIPP for MIPI CSI camera input
- Set pixel format to NV21 (YVU SEMIPLANAR 420)
- Add ISP auto exposure and white balance configuration

Closes #3
Co-Authored-By: Claude <noreply@anthropic.com>"

git commit -m "fix(rtsp): fix H.264 FU-A fragmentation for large frames

When frame size exceeds MTU(1400), correctly fragment
NAL units into FU-A packets per RFC 6184.

Fixes #12
Co-Authored-By: Claude <noreply@anthropic.com>"

git commit -m "perf(muxer): add multi-threaded MP4 writing

Implement producer-consumer pattern with 2 consumer threads
for MP4 file writing. 40% speed improvement.

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## 4. 目录结构管理

### 4.1 仓库顶层目录规划

```
tina-v853-100ask/
├── README.md                     # 项目说明
├── PROJECT_PLAN.md               # 项目详细技术文档
├── GIT_MANAGEMENT.md             # 本文档
├── .gitignore                    # Git 忽略规则
│
├── build/                        # 构建脚本 (SDK 自带)
├── device/                       # 设备配置
│   └── config/chips/v853/
│       └── configs/100ask/       # 100ask 板级配置 ★
│
├── kernel/                       # Linux 4.9 内核 (关键修改部分)
│   └── linux-4.9/
│       └── drivers/
│           ├── media/platform/
│           │   ├── sunxi-vin/    # 视频输入驱动 ★
│           │   └── sunxi-isp/    # ISP 驱动 ★
│           └── video/fbdev/sunxi/
│               └── disp2/        # 显示驱动 ★
│
├── openwrt/
│   ├── package/                  # 自定义应用程序包 ★
│   │   ├── allwinner/            # 全志特有包
│   │   └── subpackage/           # 第三方包
│   └── target/v853/
│       └── v853-100ask/          # 板级 OpenWrt 配置 ★
│
├── app/                          # ★ 我们的应用层代码 (新建)
│   ├── camera_preview/           # 摄像头预览应用
│   ├── video_encoder/            # 视频编码应用
│   ├── video_decoder/            # 视频解码应用
│   ├── audio_capture/            # 音频采集应用
│   ├── audio_playback/           # 音频播放应用
│   ├── rtsp_server/              # RTSP 服务器
│   ├── mp4_muxer/                # MP4 封装器
│   └── mp4_demuxer/              # MP4 解封装器
│
├── scripts/                      # ★ 辅助脚本 (新建)
│   ├── start_rtsp_stream.sh      # 启动 RTSP 推流
│   ├── start_recording.sh        # 启动录制
│   └── test_all_modules.sh       # 模块测试脚本
│
└── docs/                         # ★ 文档 (新建)
    ├── hardware_setup.md         # 硬件连接说明
    ├── isp_tuning_guide.md       # ISP 调优指南
    └── performance_notes.md      # 性能优化笔记
```

### 4.2 日常管理命令速查

```bash
# === 查看状态 ===
git status                       # 查看工作区状态
git log --oneline --graph -20    # 查看最近20条提交
git diff                         # 查看未暂存的变化
git diff --cached                # 查看已暂存的变化

# === 分支管理 ===
git branch                       # 列出本地分支
git branch -a                    # 列出所有分支 (含远程)
git checkout -b <branch-name>    # 创建并切换到新分支
git merge <branch-name>          # 合并分支到当前分支
git branch -d <branch-name>      # 删除本地分支

# === 暂存与提交 ===
git add <file>                   # 暂存单个文件
git add -A                       # 暂存所有变更
git add -p                       # 交互式暂存 (逐块选择)
git commit -m "message"          # 提交
git commit --amend               # 修改最后一次提交

# === 远程操作 ===
git push origin <branch>         # 推送到远程
git pull origin main             # 从 main 拉取
git fetch origin                 # 获取远程更新 (不合并)
git remote -v                    # 查看远程仓库

# === 撤销操作 ===
git checkout -- <file>           # 撤销文件修改
git reset HEAD <file>            # 取消暂存
git reset --soft HEAD~1          # 撤销最后一次提交 (保留修改)
git revert <commit-hash>         # 撤销某个提交 (安全)

# === 暂存工作 ===
git stash                        # 暂存当前工作
git stash pop                    # 恢复暂存
git stash list                   # 查看暂存列表

# === 搜索 ===
git log --grep="venc"            # 搜索提交信息
git grep "vi_attr"               # 搜索代码内容
git log --author="yyf"           # 按作者搜索
```

---

## 5. 与其他源码的协同

### 5.1 课程源码的引入

将 `/home/yyf/tina/第一期源码/` 和 `/home/yyf/tina/第二期源码/` 中的关键代码引入到仓库：

```bash
# 方式一: 直接复制到 app/ 目录
cp -r /home/yyf/tina/第一期源码/06_视频输入输出模块/6-8_绑定模式实现摄像头实时预览/sample_virvi2vo \
    app/camera_preview/
git add app/camera_preview/

# 方式二: 使用 git subtree (跨仓库共享)
# 将课程源码作为独立仓库，用 subtree 引入
```

### 5.2 SDK 原版的修改跟踪

对于 Tina SDK 原版文件的修改，使用 `.patch` 文件管理：

```bash
# 生成补丁
git diff > patches/fix-libubox-json-c-include.patch

# 应用补丁
git apply patches/fix-libubox-json-c-include.patch
```

---

## 6. 首次提交与推送

```bash
# 1. 确保在正确的分支
git checkout -b main

# 2. 添加所有应该跟踪的文件
git add .
git add PROJECT_PLAN.md GIT_MANAGEMENT.md .gitignore
git add app/ scripts/ docs/

# 3. 确认状态
git status

# 4. 首次提交
git commit -m "chore: initial commit with SDK baseline

- Add PROJECT_PLAN.md with detailed architecture doc
- Add GIT_MANAGEMENT.md with git workflow guide
- Configure .gitignore for SDK build artifacts
- Baseline: Tina SDK v853-100ask (Linux 4.9)

Co-Authored-By: Claude <noreply@anthropic.com>"

# 5. 推送到 GitHub
git push -u origin main
```

---

## 7. 注意事项

1. **永远不要提交 `out/` 目录** — 它是编译输出，包含大量二进制文件 (约 11GB)
2. **永远不要提交 `prebuilt/` 目录** — 预编译工具链约 3-5GB
3. **不要提交 `.repo/` 目录** — 这是 repo 工具的管理目录 (约 5.6GB)
4. **内核和 Buildroot 源码** — 只需跟踪你自己修改过的文件，不要跟踪整个未修改的源码树
5. **提交前用 `git status` 确认** — 确保不会误提交大文件
6. **大于 100MB 的文件使用 Git LFS** — 例如大的 .so 库文件、固件镜像等

```bash
# 检查是否有大文件
git ls-files --stage | awk '{print $4, $3}' | sort -rn -k2 | head -10
```
