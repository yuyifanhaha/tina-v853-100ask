# Tina V853 100ask — 全志 V853 音视频项目

基于全志 V853 芯片 (sun8iw21p1) 和 Tina Linux SDK 的音视频处理系统。

## 项目目标

实现完整的嵌入式音视频采集、编码、传输、存储和播放系统：

- 📷 **视频采集**: MIPI CSI 摄像头 → V4L2 → MPP VI
- 🖥️ **视频显示**: MPP VO → MIPI DSI LCD
- 🎬 **视频编解码**: H.264/H.265 硬件编解码 (VE)
- 🎤 **音频采集播放**: MIC → ALSA → AI/AO
- 📡 **RTSP 流媒体**: H.264 码流网络传输
- 💾 **MP4 封装**: 音视频流封装为 MP4 文件
- 📂 **MP4 解封装**: MP4 文件解析与播放

## 硬件平台

- **芯片**: 全志 V853 (ARM Cortex-A7 + RISC-V E907)
- **开发板**: 100ask V853 开发板
- **摄像头**: GC2053 MIPI CSI (1920×1080)
- **显示屏**: MIPI DSI LCD (1024×600)

## 软件架构

```
Application Layer ── app/* (用户应用程序)
    │
MPP Middleware ──── libmpp (全志媒体处理平台)
    │
Kernel Drivers ──── sunxi-vin / sunxi-isp / sunxi-disp / sunxi-audio
    │
Hardware ────────── V853 SoC (VE / ISP / DE / Audio Codec)
```

## 目录结构

```
tina-v853-100ask/
├── app/                    # 应用层代码
│   ├── camera_preview/     # 摄像头预览
│   ├── video_encoder/      # 视频编码
│   ├── video_decoder/      # 视频解码
│   ├── audio_capture/      # 音频采集
│   ├── audio_playback/     # 音频播放
│   ├── rtsp_server/        # RTSP 服务器
│   ├── mp4_muxer/          # MP4 封装
│   ├── mp4_demuxer/        # MP4 解封装
│   ├── g2d_rotate/         # G2D 旋转缩放
│   └── isp_tuning/         # ISP 图像调优
├── scripts/                # 辅助脚本
├── docs/                   # 文档
├── PROJECT_PLAN.md         # 详细技术文档
├── GIT_MANAGEMENT.md       # Git 管理说明
└── .gitignore
```

## 快速开始

### 编译 SDK

```bash
source build/envsetup.sh
lunch                        # 选择 1 (v853-100ask-tina)
make -j$(nproc)
pack                         # 打包固件
```

### 运行示例

```bash
# 摄像头预览
./app/camera_preview/sample_virvi2vo -path /etc/sample_virvi2vo.conf

# 视频编码
./app/video_encoder/sample_vi2venc -path /etc/sample_venc.conf

# RTSP 推流
./app/rtsp_server/rtsp_server &

# MP4 录制
./app/mp4_muxer/sample_av2muxer -path /etc/sample_av2muxer.conf
```

## 开发状态

| 模块 | 状态 | 分支 |
|------|------|------|
| SDK 编译 | ✅ 完成 | main |
| 摄像头预览 | 🔲 待开发 | feature/vi-module |
| 视频编码 | 🔲 待开发 | feature/venc-module |
| 视频解码 | 🔲 待开发 | feature/vdec-module |
| 音频采集播放 | 🔲 待开发 | feature/audio-module |
| RTSP 服务器 | 🔲 待开发 | feature/rtsp-server |
| MP4 封装 | 🔲 待开发 | feature/mp4-muxer |
| MP4 解封装 | 🔲 待开发 | feature/mp4-demuxer |
| 系统集成 | 🔲 待开发 | feature/integration |

## 参考文档

- [详细技术文档](PROJECT_PLAN.md)
- [Git 管理说明](GIT_MANAGEMENT.md)
- [V853 数据手册](../05_芯片手册/v853-amp-v853s_datasheet_v1.1.pdf)
