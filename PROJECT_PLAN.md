# 全志 V853 音视频项目 — 从驱动到应用层完整实现文档

## 项目概述

**芯片平台**: 全志 V853 (sun8iw21p1)  
**开发板**: 100ask V853 开发板  
**SDK**: Tina Linux (基于 OpenWrt + Buildroot)  
**内核版本**: Linux 4.9  
**目标**: 利用全志 MPP (Media Processing Platform) 实现 MIPI 摄像头采集、H.264/H.265 硬件编解码、RTSP 流媒体传输、音频采集播放、MP4 封装/解封装的完整音视频系统

---

## 目录

1. [系统架构概览](#1-系统架构概览)
2. [环境准备与SDK编译](#2-环境准备与sdk编译)
3. [模块一：视频输入模块 VI](#3-模块一视频输入模块-vi)
4. [模块二：视频输出模块 VO](#4-模块二视频输出模块-vo)
5. [模块三：视频编码模块 VENC](#5-模块三视频编码模块-venc)
6. [模块四：视频解码模块 VDEC](#6-模块四视频解码模块-vdec)
7. [模块五：音频输入输出模块 AI/AO](#7-模块五音频输入输出模块-aiao)
8. [模块六：音频编解码模块 AENC/ADEC](#8-模块六音频编解码模块-aencadec)
9. [模块七：RTSP 流媒体传输](#9-模块七rtsp-流媒体传输)
10. [模块八：MP4 封装与解封装](#10-模块八mp4-封装与解封装)
11. [模块九：图像处理与叠加 (G2D/ISP/OSD)](#11-模块九图像处理与叠加-g2disposd)
12. [模块十：系统集成与性能优化](#12-模块十系统集成与性能优化)
13. [现有源码对照表](#13-现有源码对照表)
14. [开发路线图](#14-开发路线图)

---

## 1. 系统架构概览

### 1.1 全志 MPP 框架

全志 MPP (Media Processing Platform) 是芯片内部的媒体处理中间件，提供统一的 API 接口来操作硬件模块。核心概念：

```
┌─────────────────────────────────────────────────────────────────┐
│                        Application Layer                         │
│                     (用户应用程序)                                │
├─────────────────────────────────────────────────────────────────┤
│              MPP API (libmpp.so / libmedia_mpp.so)              │
│    ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐     │
│    │  VI  │ │  VO  │ │ VENC │ │ VDEC │ │  AI  │ │  AO  │     │
│    │Video │ │Video │ │Video │ │Video │ │Audio │ │Audio │     │
│    │Input │ │Output│ │Encode│ │Decode│ │Input │ │Output│     │
│    └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘     │
│       │        │        │        │        │        │          │
│  ┌────┴────────┴────────┴────────┴────────┴────────┴────┐    │
│  │              MPP System (mpi_sys)                      │    │
│  │          通道绑定 (Bind) / 内存管理 (ion)               │    │
│  └────────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────────┤
│                      Linux Kernel Layer                          │
│  ┌────────┐ ┌────────┐ ┌──────────┐ ┌──────┐ ┌──────────┐    │
│  │sunxi-  │ │sunxi-  │ │sunxi-ve   │ │sunxi-│ │sunxi-audio│    │
│  │vin     │ │disp    │ │(H.264/265 │ │g2d   │ │(ALSA)     │    │
│  │(V4L2)  │ │(DRM/FB)│ │ HW codec) │ │      │ │           │    │
│  └────────┘ └────────┘ └──────────┘ └──────┘ └──────────┘    │
├─────────────────────────────────────────────────────────────────┤
│                        Hardware Layer                             │
│  MIPI CSI → ISP → VI    |  Display Engine → MIPI DSI/LCD        │
│  VE (H.264/H.265 Codec) |  Audio Codec → MIC/Lineout            │
│  G2D (2D Accelerator)   |  DMA/ION Memory                       │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 MPP 通道模型

MPP 采用**设备-通道 (Dev-Chn)** 模型：

| 模块 | 设备 ID | 通道 ID 范围 | 说明 |
|------|---------|-------------|------|
| VI (Video Input) | VIPP 0-3 | Chn 0-1 | 每个 VIPP 对应一个摄像头输入 |
| VO (Video Output) | Dev 0 | Layer 0-7, Chn 0-3 | Layer 对应显示层，Chn 对应视频通道 |
| VENC (Video Encoder) | Dev 0 | Chn 0-5 | 硬件 H.264/H.265/JPEG 编码器 |
| VDEC (Video Decoder) | Dev 0 | Chn 0-2 | 硬件 H.264/H.265/JPEG 解码器 |
| AI (Audio Input) | Dev 0 | Chn 0-2 | 音频采集 (MIC) |
| AO (Audio Output) | Dev 0 | Chn 0-2 | 音频播放 (Lineout/Speaker) |
| AENC (Audio Encoder) | Dev 0 | Chn 0-2 | 音频编码 (AAC/G.711 等) |
| ADEC (Audio Decoder) | Dev 0 | Chn 0-2 | 音频解码 |

### 1.3 Bind 绑定模式

MPP 支持通道间自动数据传递（零拷贝），无需应用层手动搬运：

```
VI Chn → [Bind] → VO Chn          # 摄像头预览
VI Chn → [Bind] → VENC Chn        # 摄像头编码
AI Chn → [Bind] → AENC Chn        # 音频采集编码
VDEC Chn → [Bind] → VO Chn        # 解码播放
```

---

## 2. 环境准备与 SDK 编译

### 2.1 系统要求

- **操作系统**: Ubuntu 18.04 或 20.04 (64-bit)
- **必需软件包**:
```bash
sudo apt install -y \
    build-essential gcc g++ gawk libncurses-dev \
    python-is-python3 git cmake autoconf automake \
    libtool flex bison bc u-boot-tools \
    libssl-dev libjson-c-dev
```

### 2.2 SDK 目录结构

```
tina-v853-open/
├── brandy/          # U-Boot 引导程序
├── build/           # 构建脚本和配置
├── buildroot/       # Buildroot 根文件系统
├── device/          # 设备配置文件 (芯片/板级配置)
├── kernel/          # Linux 4.9 内核
├── openwrt/         # OpenWrt 系统 (应用层包管理)
├── out/             # 编译输出
├── platform/        # 平台相关源码
├── prebuilt/        # 预编译工具链和根文件系统
└── tools/           # 工具链
```

### 2.3 编译流程

```bash
# 1. 加载环境
source build/envsetup.sh

# 2. 选择配置
lunch
# 选择: 1 (v853-100ask-tina)

# 3. 编译 (约 60 分钟)
make -j$(nproc)

# 4. 打包固件
pack
```

---

## 3. 模块一：视频输入模块 (VI)

### 3.1 功能描述

视频输入模块负责接收 MIPI CSI 摄像头的图像数据，经过 ISP 图像信号处理器进行色彩校正、白平衡、曝光控制等处理后输出 YUV 格式视频帧。

### 3.2 硬件通路

```
MIPI CSI Camera → MIPI Controller → ISP (Image Signal Processor)
    → VIPP (Video Input Post Processor) → DDR Memory (ion buffer)
```

### 3.3 涉及的内核驱动

| 驱动 | 路径 | 功能 |
|------|------|------|
| sunxi-vin | `kernel/linux-4.9/drivers/media/platform/sunxi-vin/` | V4L2 视频采集框架 |
| sunxi-isp | `kernel/linux-4.9/drivers/media/platform/sunxi-isp/` | ISP 图像处理 |
| GC2053 sensor | `kernel/linux-4.9/drivers/media/platform/sunxi-vin/modules/sensor/gc2053_mipi.c` | MIPI 摄像头驱动 |

### 3.4 MPP API 调用流程

```c
// 1. 初始化 MPP 系统
MPP_SYS_CONF_S sys_conf;
sys_conf.nAlignWidth = 32;
AW_MPI_SYS_SetConf(&sys_conf);
AW_MPI_SYS_Init();

// 2. 创建 VIPP 设备
int vipp_dev = 0;
AW_MPI_VI_CreateVipp(vipp_dev);

// 3. 注册回调
MPPCallbackInfo cb_info;
cb_info.cookie = context;
cb_info.callback = vi_callback;
AW_MPI_VI_RegisterCallback(vipp_dev, &cb_info);

// 4. 配置 VIPP 属性 (分辨率、格式、帧率)
VI_ATTR_S vi_attr;
memset(&vi_attr, 0, sizeof(VI_ATTR_S));
vi_attr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
vi_attr.memtype = V4L2_MEMORY_MMAP;
vi_attr.format.pixelformat = V4L2_PIX_FMT_NV21;   // 或 NV12
vi_attr.format.width = 1920;
vi_attr.format.height = 1080;
vi_attr.nbufs = 5;
vi_attr.nplanes = 2;
vi_attr.fps = 30;
vi_attr.drop_frame_num = 0;
AW_MPI_VI_SetVippAttr(vipp_dev, &vi_attr);

// 5. 启动 ISP
AW_MPI_ISP_Run(isp_dev);

// 6. 创建虚拟通道 (VIPP → VI Chn)
int vi_chn = 0;
AW_MPI_VI_CreateVirChn(vipp_dev, vi_chn, NULL);

// 7. 启用 VIPP (开始采集)
AW_MPI_VI_EnableVipp(vipp_dev);

// 8. ISP 图像调优 (色彩还原)
AW_MPI_ISP_SetBrightness(isp_dev, 128);   // 亮度 0-256
AW_MPI_ISP_SetContrast(isp_dev, 128);     // 对比度 0-256
AW_MPI_ISP_SetSaturation(isp_dev, 150);   // 饱和度 0-256 (提高20%)
AW_MPI_ISP_SetSharpness(isp_dev, 140);    // 锐度 0-256
AW_MPI_ISP_SetHue(isp_dev, 128);          // 色调
AW_MPI_ISP_AE_SetMode(isp_dev, 1);        // 自动曝光
AW_MPI_ISP_AWB_SetMode(isp_dev, 1);       // 自动白平衡

// 9. 启用 VI 通道
AW_MPI_VI_EnableVirChn(vipp_dev, vi_chn);

// 10. 获取视频帧
VIDEO_FRAME_INFO_S frame_info;
AW_MPI_VI_GetFrame(vipp_dev, vi_chn, &frame_info, 1000);
// ... 处理 frame_info ...
AW_MPI_VI_ReleaseFrame(vipp_dev, vi_chn, &frame_info);

// 11. 清理
AW_MPI_VI_DisableVirChn(vipp_dev, vi_chn);
AW_MPI_VI_DestroyVirChn(vipp_dev, vi_chn);
AW_MPI_ISP_Stop(isp_dev);
AW_MPI_VI_DisableVipp(vipp_dev);
AW_MPI_VI_DestroyVipp(vipp_dev);
AW_MPI_SYS_Exit();
```

### 3.5 关键调优参数

| 参数 | 调整范围 | 效果 | 简历目标 |
|------|---------|------|----------|
| Brightness | 0-256 | 亮度控制 | - |
| Contrast | 0-256 | 对比度控制 | - |
| Saturation | 0-256 | 色彩饱和度 | **色彩还原度提升 20%** |
| Sharpness | 0-256 | 图像锐化 | - |
| AE Mode | 0/1 | 自动曝光模式 | - |
| AWB Mode | 0/1 | 自动白平衡 | - |
| WDR Mode | 0/1 | 宽动态范围 | - |
| fps | 1-60 | 帧率控制 | **码流稳定性提高 30%** |
| drop_frame_num | 0-10 | 丢帧数 | - |

### 3.6 实现要点

1. **摄像头探测**: 使用 `media-ctl` 工具确定 `/dev/videoX` 设备节点
2. **像素格式**: 推荐 NV21 (YVU SEMIPLANAR 420) 或 NV12，这是芯片硬件加速的格式
3. **色彩还原度提升**: 主要在 ISP 层面调整 Saturation、AWB、ColorTemp 参数
4. **码流稳定性**: 设置合适帧率 (fps)、缓冲区数量 (nbufs=5~8)、丢帧策略 (drop_frame_num)

---

## 4. 模块二：视频输出模块 (VO)

### 4.1 功能描述

将视频帧输出到 MIPI DSI / LCD 显示屏，支持多层叠加、缩放、裁剪。

### 4.2 硬件通路

```
DDR Memory → Display Engine (DE) → Layer Mixer → MIPI DSI → LCD Panel
```

### 4.3 涉及的内核驱动

| 驱动 | 路径 | 功能 |
|------|------|------|
| sunxi-disp2 | `kernel/linux-4.9/drivers/video/fbdev/sunxi/disp2/` | 显示引擎 (DE) |
| sunxi-lcd | `kernel/linux-4.9/drivers/video/fbdev/sunxi/disp2/disp/lcd/` | LCD 面板驱动 |
| hwdisplay | `kernel/linux-4.9/drivers/video/fbdev/sunxi/hwdisplay/` | 硬件显示接口 |

### 4.4 MPP API 调用流程

```c
// 1. 启用 VO 设备
int vo_dev = 0;
AW_MPI_VO_Enable(vo_dev);

// 2. 配置显示接口类型和时序
VO_PUB_ATTR_S pub_attr;
AW_MPI_VO_GetPubAttr(vo_dev, &pub_attr);
pub_attr.enIntfType = VO_INTF_LCD;      // LCD 接口
pub_attr.enIntfSync = VO_OUTPUT_NTSC;   // 时序 (或自定义分辨率)
AW_MPI_VO_SetPubAttr(vo_dev, &pub_attr);

// 3. 添加 UI 层 (用于 OSD 叠加)
int ui_layer = HLAY(2, 0);             // 硬件层 2, 通道 0
AW_MPI_VO_AddOutsideVideoLayer(ui_layer);
AW_MPI_VO_CloseVideoLayer(ui_layer);

// 4. 启用视频层
int video_layer = HLAY(0, 0);          // 硬件层 0, 通道 0
AW_MPI_VO_EnableVideoLayer(video_layer);

// 5. 设置视频层的显示区域
VO_VIDEO_LAYER_ATTR_S layer_attr;
AW_MPI_VO_GetVideoLayerAttr(video_layer, &layer_attr);
layer_attr.stDispRect.X = 0;
layer_attr.stDispRect.Y = 0;
layer_attr.stDispRect.Width = 1024;    // 显示宽度
layer_attr.stDispRect.Height = 600;    // 显示高度
AW_MPI_VO_SetVideoLayerAttr(video_layer, &layer_attr);

// 6. 创建 VO 通道
int vo_chn = 0;
AW_MPI_VO_CreateChn(video_layer, vo_chn);

// 7. 注册回调 (用于帧释放通知)
MPPCallbackInfo cb_info;
cb_info.cookie = context;
cb_info.callback = vo_callback;
AW_MPI_VO_RegisterCallback(video_layer, vo_chn, &cb_info);

// 8. 设置显示缓冲区数
AW_MPI_VO_SetChnDispBufNum(video_layer, vo_chn, 2);

// 9. 绑定 VI → VO (零拷贝预览)
MPP_CHN_S vi_chn = {MOD_ID_VIU, vipp_dev, vi_chn_idx};
MPP_CHN_S vo_chn_s = {MOD_ID_VOU, video_layer, vo_chn};
AW_MPI_SYS_Bind(&vi_chn, &vo_chn_s);

// 10. 启动 VO 通道
AW_MPI_VO_StartChn(video_layer, vo_chn);

// 11. 清理
AW_MPI_VO_StopChn(video_layer, vo_chn);
AW_MPI_SYS_UnBind(&vi_chn, &vo_chn_s);
AW_MPI_VO_DestroyChn(video_layer, vo_chn);
AW_MPI_VO_DisableVideoLayer(video_layer);
AW_MPI_VO_Disable(vo_dev);
```

### 4.5 解决画面闪烁和色彩偏差

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 画面闪烁 | 缓冲区不足 | SetChnDispBufNum ≥ 2 |
| 画面闪烁 | 帧率不匹配 | VO 同步信号与摄像头帧率一致 |
| 色彩偏差 | YUV→RGB 转换矩阵错误 | 确认像素格式映射正确 |
| 色彩偏差 | 显示时序不对 | 检查 LCD datasheet 调整时序参数 |
| 撕裂 | 没有垂直同步 | 使用 FBIOPAN_DISPLAY ioctl |

---

## 5. 模块三：视频编码模块 (VENC)

### 5.1 功能描述

利用芯片内部 VE (Video Engine) 硬件编码器将 YUV 视频帧编码为 H.264 / H.265 码流。

### 5.2 硬件通路

```
VI Chn → [Bind] → VENC Chn → Encoded Bitstream (H.264/H.265)
```

或手动模式：
```
VI GetFrame → YUV Data → VENC SendFrame → GetStream → H.264/H.265 Bitstream
```

### 5.3 MPP API 调用流程

```c
// 1. 创建 VENC 通道
int venc_chn = 0;
VENC_CHN_ATTR_S venc_attr;
memset(&venc_attr, 0, sizeof(venc_attr));
venc_attr.VeAttr.Type = VENC_CODEC_H264;  // 或 VENC_CODEC_H265
venc_attr.VeAttr.AttrH264.u32Width = 1920;
venc_attr.VeAttr.AttrH264.u32Height = 1080;
venc_attr.VeAttr.AttrH264.u32FrameRate = 30;
venc_attr.VeAttr.AttrH264.u32BitRate = 4000000;  // 4Mbps
venc_attr.VeAttr.AttrH264.u32Profile = 1;        // Main Profile
venc_attr.VeAttr.AttrH264.u32Level = 40;         // Level 4.0
venc_attr.VeAttr.AttrH264.u32Gop = 30;           // GOP size
venc_attr.VeAttr.AttrH264.bByFrame = TRUE;
AW_MPI_VENC_CreateChn(venc_chn, &venc_attr);

// 2. 启动编码通道
AW_MPI_VENC_StartChn(venc_chn);

// 3. 绑定 VI → VENC (零拷贝)
MPP_CHN_S vi_chn = {MOD_ID_VIU, vipp_dev, vi_chn_idx};
MPP_CHN_S venc_chn_s = {MOD_ID_VENC, 0, venc_chn};
AW_MPI_SYS_Bind(&vi_chn, &venc_chn_s);

// 4. 获取编码后的码流
VENC_STREAM_S venc_stream;
VENC_PACK_S pack;
AW_MPI_VENC_GetStream(venc_chn, &venc_stream, 1000);
// ... 处理 venc_stream (发送RTSP/写文件) ...
AW_MPI_VENC_ReleaseStream(venc_chn, &venc_stream);

// 5. 动态调整码率和帧率
VENC_RC_PARAM_S rc_param;
AW_MPI_VENC_GetRcParam(venc_chn, &rc_param);
rc_param.u32BitRate = 3000000;  // 动态降到 3Mbps
rc_param.u32FrameRate = 15;     // 动态降到 15fps
AW_MPI_VENC_SetRcParam(venc_chn, &rc_param);

// 6. QPMAP 码流控制 (精细化调节)
VENC_QPMAP_S qp_map;
qp_map.u32AbsQp = 30;           // 量化参数 (越低质量越高)
AW_MPI_VENC_SetQpMap(venc_chn, &qp_map);
```

### 5.4 H.264 vs H.265

| 特性 | H.264 | H.265 |
|------|-------|-------|
| 压缩率 | 基准 | 比 H.264 高 40-50% |
| 编码复杂度 | 低 | 高 |
| 兼容性 | 广泛 | 较新 |
| V853 支持 | ✅ 硬件 | ✅ 硬件 |

### 5.5 码率控制策略

| 模式 | 说明 | 适用场景 |
|------|------|----------|
| CBR (固定码率) | 恒定比特率 | RTSP 流媒体 |
| VBR (可变码率) | 画质优先 | 本地存储 |
| QPMAP | 精细量化控制 | 定制化画质需求 |

---

## 6. 模块四：视频解码模块 (VDEC)

### 6.1 功能描述

利用硬件解码器将 H.264/H.265 码流解码为 YUV 视频帧用于播放。

### 6.2 MPP API 调用流程

```c
// 1. 创建 VDEC 通道
int vdec_chn = 0;
VDEC_CHN_ATTR_S vdec_attr;
memset(&vdec_attr, 0, sizeof(vdec_attr));
vdec_attr.mType = VDEC_CODEC_H264;
vdec_attr.mPicWidth = 1920;
vdec_attr.mPicHeight = 1080;
vdec_attr.mFrameRate = 30;
vdec_attr.mOutputPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
AW_MPI_VDEC_CreateChn(vdec_chn, &vdec_attr);

// 2. 启动解码通道
AW_MPI_VDEC_StartChn(vdec_chn);

// 3. 发送编码数据 (从 RTSP/文件读取)
VDEC_STREAM_S vdec_stream;
vdec_stream.pu8Addr = h264_data;
vdec_stream.u32Len = data_len;
vdec_stream.bEndOfStream = FALSE;
AW_MPI_VDEC_SendStream(vdec_chn, &vdec_stream, 1000);

// 4. 获取解码帧
VIDEO_FRAME_INFO_S frame_info;
AW_MPI_VDEC_GetFrame(vdec_chn, &frame_info, 1000);
// ... 显示或处理 frame_info ...
AW_MPI_VDEC_ReleaseFrame(vdec_chn, &frame_info);

// 5. 绑定 VDEC → VO (解码后直接显示)
MPP_CHN_S vdec_chn_s = {MOD_ID_VDEC, 0, vdec_chn};
MPP_CHN_S vo_chn_s = {MOD_ID_VOU, video_layer, vo_chn};
AW_MPI_SYS_Bind(&vdec_chn_s, &vo_chn_s);
```

---

## 7. 模块五：音频输入输出模块 (AI/AO)

### 7.1 功能描述

对接 ALSA 音频框架和全志内部音频 CODEC，实现 MIC 拾音采集 PCM 数据，以及通过 Lineout/喇叭播放音频。

### 7.2 硬件通路

```
MIC → ADC → Audio Codec → AI (Audio Input) → DDR Memory (PCM Buffer)
DDR Memory (PCM Buffer) → AO (Audio Output) → Audio Codec → DAC → Lineout/Speaker
```

### 7.3 涉及的内核驱动

| 驱动 | 路径 | 功能 |
|------|------|------|
| sunxi-audio | `kernel/linux-4.9/sound/soc/sunxi/` | ALSA SoC 音频驱动 |
| sunxi-codec | `kernel/linux-4.9/sound/soc/sunxi/sunxi-codec.c` | 内部音频 CODEC |
| sunxi-daudio | `kernel/linux-4.9/sound/soc/sunxi/sunxi-daudio.c` | 数字音频接口 |

### 7.4 MPP API 调用流程

#### 音频采集 (AI)

```c
// 1. 设置 AI 属性
AI_ATTR_S ai_attr;
ai_attr.enSamplerate = AUDIO_SAMPLE_RATE_16000;  // 16kHz
ai_attr.enBitwidth = AUDIO_BIT_WIDTH_16;
ai_attr.enSoundmode = AUDIO_SOUND_MODE_MONO;     // 单声道
ai_attr.enWorkmode = AIO_MODE_I2S_MASTER;
ai_attr.u32ChnCnt = 1;
ai_attr.u32FrameNum = 30;                        // 帧数 (缓冲区)
ai_attr.u32PtNumPerFrm = 1024;                   // 每帧采样点数

// 2. 创建 AI 通道
int ai_dev = 0, ai_chn = 0;
AW_MPI_AI_CreateChn(ai_dev, ai_chn, &ai_attr);

// 3. 启用 AI 通道
AW_MPI_AI_EnableChn(ai_dev, ai_chn);

// 4. 获取 PCM 音频帧
AUDIO_FRAME_S audio_frame;
AW_MPI_AI_GetFrame(ai_dev, ai_chn, &audio_frame, NULL, 1000);
// ... 编码或处理音频帧 ...
AW_MPI_AI_ReleaseFrame(ai_dev, ai_chn, &audio_frame, NULL);
```

#### 音频播放 (AO)

```c
// 1. 设置 AO 属性
AO_ATTR_S ao_attr;
ao_attr.enSamplerate = AUDIO_SAMPLE_RATE_16000;
ao_attr.enBitwidth = AUDIO_BIT_WIDTH_16;
ao_attr.enSoundmode = AUDIO_SOUND_MODE_MONO;
ao_attr.enWorkmode = AIO_MODE_I2S_MASTER;
ao_attr.u32ChnCnt = 1;

// 2. 创建 AO 通道
int ao_dev = 0, ao_chn = 0;
AW_MPI_AO_CreateChn(ao_dev, ao_chn, &ao_attr);

// 3. 发送音频帧播放
AW_MPI_AO_SendFrame(ao_dev, ao_chn, &audio_frame, 1000);
```

### 7.5 ALSA 对接

Mpp 音频框架底层封装了 ALSA，也可以直接使用 ALSA API：

```c
// ALSA 原生方式
snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);
snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE,
    SND_PCM_ACCESS_RW_INTERLEAVED, 1, 16000, 0, 500000);
snd_pcm_readi(handle, buffer, frames);
```

### 7.6 音频质量优化

| 参数 | 推荐值 | 效果 |
|------|--------|------|
| 采样率 | 16000 / 48000 Hz | 16kHz 适合语音，48kHz 适合音乐 |
| 位宽 | 16 bit | 标准 CD 音质 |
| 缓冲区帧数 | 30-50 | 减少丢帧 |
| 每帧采样点 | 1024 | AAC 编码标准帧大小 |
| 降噪 | ISP/软件降噪 | **失真率降至 5% 以下** |

---

## 8. 模块六：音频编解码模块 (AENC/ADEC)

### 8.1 功能描述

将 PCM 音频数据编码为 AAC 等压缩格式，或将压缩音频解码为 PCM 用于播放。

### 8.2 MPP API 调用流程

#### 音频编码 (AENC)

```c
// 1. 创建 AENC 通道
int aenc_chn = 0;
AENC_CHN_ATTR_S aenc_attr;
aenc_attr.enType = AENC_TYPE_AAC;        // AAC 编码
aenc_attr.u32SampleRate = 16000;
aenc_attr.u32Channels = 1;
aenc_attr.u32BitRate = 32000;           // 32kbps
AW_MPI_AENC_CreateChn(aenc_chn, &aenc_attr);

// 2. 绑定 AI → AENC (零拷贝)
MPP_CHN_S ai_chn_s = {MOD_ID_AI, ai_dev, ai_chn};
MPP_CHN_S aenc_chn_s = {MOD_ID_AENC, 0, aenc_chn};
AW_MPI_SYS_Bind(&ai_chn_s, &aenc_chn_s);

// 3. 获取编码后的音频流
AENC_STREAM_S aenc_stream;
AW_MPI_AENC_GetStream(aenc_chn, &aenc_stream, 1000);
// ... 封装或发送 ...
AW_MPI_AENC_ReleaseStream(aenc_chn, &aenc_stream);
```

#### 音频解码 (ADEC)

```c
// 1. 创建 ADEC 通道
int adec_chn = 0;
ADEC_CHN_ATTR_S adec_attr;
adec_attr.enType = ADEC_TYPE_AAC;
adec_attr.u32SampleRate = 16000;
adec_attr.u32Channels = 1;
AW_MPI_ADEC_CreateChn(adec_chn, &adec_attr);

// 2. 发送压缩音频流
ADEC_STREAM_S adec_stream;
adec_stream.pu8Addr = aac_data;
adec_stream.u32Len = data_len;
AW_MPI_ADEC_SendStream(adec_chn, &adec_stream, 1000);

// 3. 获取解码后的 PCM 帧
AUDIO_FRAME_S pcm_frame;
AW_MPI_ADEC_GetFrame(adec_chn, &pcm_frame, 1000);
```

---

## 9. 模块七：RTSP 流媒体传输

### 9.1 功能描述

基于 RTSP/RTP/RTCP 协议实现 H.264/H.265 视频流的网络传输，支持多客户端并发访问。

### 9.2 协议栈

```
Application     : RTSP Server (Live555 / 自实现)
Transport       : RTP (Real-time Transport Protocol) / RTCP
Network         : UDP / TCP (Interleaved)
```

### 9.3 实现方式

#### 方案 A：自实现 RTSP 服务器

参考现有源码 `08_RTSP流媒体协议/`，实现轻量级 RTSP 服务器：

```c
// 核心流程
// 1. TCP Server 监听 554 端口
// 2. 解析 RTSP 请求 (OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN)
// 3. RTP 打包 H.264/H.265 NAL 单元
// 4. UDP 发送 RTP 包到客户端

// RTP 打包 H.264 (RFC 6184)
typedef struct {
    uint8_t  v_p_x_cc;      // version, padding, extension, csrc count
    uint8_t  m_pt;          // marker, payload type
    uint16_t sequence;      // sequence number
    uint32_t timestamp;     // timestamp
    uint32_t ssrc;          // synchronization source
} RTP_HEADER;

// 发送 H.264 NAL through RTP
void send_h264_nal(const uint8_t *nal_data, int nal_len, int is_keyframe) {
    if (nal_len <= MAX_RTP_PAYLOAD) {
        // 单包模式 (Single NAL Unit)
        send_rtp_packet(nal_data, nal_len, is_keyframe);
    } else {
        // 分片模式 (FU-A Fragmentation)
        send_fu_a_packets(nal_data, nal_len, is_keyframe);
    }
}
```

#### 方案 B：使用 Live555 库

```cpp
// Live555 集成
#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>

// 创建 H264 RTSP Server
RTSPServer* rtspServer = RTSPServer::createNew(*env, 554, NULL);
ServerMediaSession* sms = ServerMediaSession::createNew(*env,
    "h264", "H.264 Video Stream", "Session streamed by V853");
sms->addSubsession(H264VideoFileServerMediaSubsession::createNew(*env,
    "test.264", False));
rtspServer->addServerMediaSession(sms);
```

### 9.4 关键实现点

| 要点 | 说明 |
|------|------|
| SDP 描述 | 正确描述 H.264 profile-level-id, sprop-parameter-sets |
| RTP 时间戳 | H.264 用 90kHz 时钟, H.265 同样 |
| FU-A 分片 | 单帧大于 MTU(1400) 时需要分片 |
| NAL 单元类型 | 正确识别 SPS/PPS/IDR/Non-IDR |
| 端口复用 | RTSP TCP Interleaved 模式绕过防火墙 |

---

## 10. 模块八：MP4 封装与解封装

### 10.1 功能描述

将 H.264/H.265 视频流和 AAC 音频流封装为 MP4 文件，或从 MP4 文件中提取音视频码流。

### 10.2 MP4 文件格式

```
MP4 Box Structure:
┌──────────────┐
│  ftyp (File Type)          │
├──────────────┤
│  moov (Movie Metadata)     │
│  ├── mvhd (Movie Header)   │
│  ├── trak (Video Track)   │
│  │   ├── tkhd              │
│  │   └── mdia              │
│  │       ├── mdhd           │
│  │       ├── hdlr           │
│  │       └── minf           │
│  │           ├── vmhd       │
│  │           ├── dinf       │
│  │           └── stbl       │
│  │               ├── stsd   │ (AVC1/HVC1 box)
│  │               ├── stts   │
│  │               ├── stss   │ (sync samples)
│  │               ├── stsc   │
│  │               ├── stsz   │
│  │               └── stco   │
│  └── trak (Audio Track)    │
│      └── ...               │
├──────────────┤
│  mdat (Media Data)         │
│  (interleaved video+audio) │
└──────────────┘
```

### 10.3 封装实现 (Muxer)

参考现有源码 `14-6_音视频流封装MP4文件/`：

```c
// 核心流程 (基于全志 MPP Muxer 或 FFmpeg libavformat)

// 方案 A：MPP 原生 Muxer
MUXER_GRP_ATTR_S muxer_attr;
muxer_attr.enType = MUXER_TYPE_MP4;
muxer_attr.u32MaxFileSize = 0;  // 不限制文件大小
AW_MPI_MUXER_CreateGrp(&muxer_grp, &muxer_attr);

// 绑定 VENC → Muxer, AENC → Muxer
MPP_CHN_S venc_chn_s = {MOD_ID_VENC, 0, venc_chn};
MPP_CHN_S aenc_chn_s = {MOD_ID_AENC, 0, aenc_chn};
MPP_CHN_S muxer_chn_s = {MOD_ID_MUXER, 0, muxer_grp};
AW_MPI_SYS_Bind(&venc_chn_s, &muxer_chn_s);
AW_MPI_SYS_Bind(&aenc_chn_s, &muxer_chn_s);

// 方案 B：FFmpeg libavformat
#include <libavformat/avformat.h>
AVFormatContext *fmt_ctx;
avformat_alloc_output_context2(&fmt_ctx, NULL, "mp4", output_path);
AVStream *video_stream = avformat_new_stream(fmt_ctx, NULL);
video_stream->codecpar->codec_id = AV_CODEC_ID_H264;
// ... 写入 SPS/PPS 到 extradata ...
avformat_write_header(fmt_ctx, NULL);
// 循环: av_write_frame(fmt_ctx, &pkt);
av_write_trailer(fmt_ctx);
```

### 10.4 多线程封装加速

```c
// 生产者-消费者模型
// 生产者线程: 从 VENC GetStream → push 到队列
// 消费者线程(×2): 从队列 pop → 写入 MP4 文件

pthread_t prod_thread, cons_thread1, cons_thread2;
pthread_mutex_t mutex;
ring_buffer_t *rb = ring_buffer_create(64); // 64 帧缓冲

// 生产者
void *producer_thread(void *arg) {
    while (running) {
        VENC_STREAM_S stream;
        AW_MPI_VENC_GetStream(venc_chn, &stream, 1000);
        ring_buffer_push(rb, &stream);  // 非阻塞
    }
}

// 消费者 (可启动 2-4 个)
void *consumer_thread(void *arg) {
    while (running) {
        VENC_STREAM_S *stream = ring_buffer_pop(rb);  // 非阻塞
        if (stream) {
            // 写入 MP4 文件
            write_mp4_frame(muxer, stream);
            AW_MPI_VENC_ReleaseStream(venc_chn, stream);
        }
    }
}
```

### 10.5 解封装实现 (Demuxer)

参考现有源码 `14-9_MP4文件解封装音视频码流/`：

```c
// MP4 解封装核心流程
// 1. 解析 ftyp box
// 2. 解析 moov box → 获取音视频轨道信息 (编码格式、分辨率、时间基)
// 3. 解析 mdat box → 读取音视频交织数据
// 4. 根据 stco/stsc/stsz/stss 表定位每一帧
// 5. 输出原始 H.264/H.265 码流 + AAC 码流

// 或使用 FFmpeg:
AVFormatContext *fmt_ctx = NULL;
avformat_open_input(&fmt_ctx, input_path, NULL, NULL);
avformat_find_stream_info(fmt_ctx, NULL);
// 遍历 streams, 找到 video_stream_idx, audio_stream_idx
while (av_read_frame(fmt_ctx, &pkt) >= 0) {
    if (pkt.stream_index == video_stream_idx) {
        // 视频帧, 发送到 VDEC 解码
    } else if (pkt.stream_index == audio_stream_idx) {
        // 音频帧, 发送到 ADEC 解码
    }
    av_packet_unref(&pkt);
}
```

### 10.6 音画同步策略

| 方案 | 说明 |
|------|------|
| PTS 对齐 | 基于 PTS (Presentation Timestamp) 同步 |
| 音频为主 | 音频时钟作为主时钟，视频追赶音频 |
| 视频为主 | 视频时钟作为主时钟，音频追赶视频 |
| NTP 同步 | RTSP 场景下用 NTP 同步 |

---

## 11. 模块九：图像处理与叠加 (G2D/ISP/OSD)

### 11.1 G2D 2D 图形加速

```c
// G2D 图像旋转、缩放、裁剪
G2D_BLT_S blt;
memset(&blt, 0, sizeof(blt));
blt.srcImg.mWidth = 1920;
blt.srcImg.mHeight = 1080;
blt.srcImg.mPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
blt.srcImg.mpPhyAddr[0] = src_phy_addr;
blt.srcImg.mStride[0] = 1920;

blt.dstImg.mWidth = 640;
blt.dstImg.mHeight = 360;
blt.dstImg.mPixelFormat = MM_PIXEL_FORMAT_RGB_8888;
blt.dstImg.mpPhyAddr[0] = dst_phy_addr;
blt.dstImg.mStride[0] = 640 * 4;

blt.rotate = G2D_ROTATE_90;  // 旋转 90°
AW_MPI_G2D_BitBlit(&blt);
```

### 11.2 ISP 图像调优

```c
// ISP 参数控制 (色彩还原度提升 20%)
AW_MPI_ISP_SetBrightness(isp_dev, 128);   // 亮度
AW_MPI_ISP_SetContrast(isp_dev, 140);     // 对比度提高
AW_MPI_ISP_SetSaturation(isp_dev, 160);   // 饱和度提高 ~20%
AW_MPI_ISP_SetSharpness(isp_dev, 150);    // 锐度提高
AW_MPI_ISP_SetHue(isp_dev, 128);          // 色调
AW_MPI_ISP_AE_SetMode(isp_dev, 1);        // 自动曝光
AW_MPI_ISP_AE_SetExposureBias(isp_dev, 4); // 曝光补偿
AW_MPI_ISP_AWB_SetMode(isp_dev, 1);       // 自动白平衡
AW_MPI_ISP_AWB_SetColorTemp(isp_dev, 5);  // 色温 6500K
AW_MPI_ISP_SetWdrMode(isp_dev, 1);        // WDR 宽动态
AW_MPI_ISP_SwitchIspConfig(isp_dev, NORMAL_CFG); // 切换 ISP 配置
```

### 11.3 OSD 叠加

```c
// OSD (On-Screen Display) 位图叠加
// 使用 VO 的 UI Layer 实现
// 参考 sample_virvi2vo.c 中的 CreateTestUILayer() 函数
// 核心步骤:
// 1. AW_MPI_VO_EnableVideoLayer(ui_layer)
// 2. 分配 ion 内存, 填充 RGBA 位图数据
// 3. AW_MPI_VO_SendFrame(ui_layer, vo_chn, &frame, 0)
```

---

## 12. 模块十：系统集成与性能优化

### 12.1 完整数据流

```
┌──────────────────────────────────────────────────────────────────────┐
│                        完整音视频处理流程                              │
│                                                                       │
│  [Camera] ──→ [VI] ──→ [ISP] ──→ [VIPP] ──┬──→ [VENC] ──→ [RTSP]   │
│                                            │                │         │
│                                            │                ├──→ [MP4]│
│                                            │                │         │
│                                            └──→ [VO] ──→ LCD│         │
│                                                                       │
│  [MIC] ──→ [AI] ──→ [AENC] ──┬──→ [RTSP Audio]                       │
│                               ├──→ [MP4 Audio Track]                  │
│                               └──→ [AO] ──→ [Speaker]                 │
│                                                                       │
│  [MP4 File] ──→ [Demuxer] ──┬──→ [VDEC] ──→ [VO] ──→ LCD            │
│                              └──→ [ADEC] ──→ [AO] ──→ Speaker        │
└──────────────────────────────────────────────────────────────────────┘
```

### 12.2 性能优化要点

| 优化项 | 方法 | 效果 |
|--------|------|------|
| 零拷贝 | MPP Bind 模式 (VI→VENC, VI→VO) | 减少 CPU 参与 |
| ION 内存 | 使用连续物理内存 (ion_alloc) | DMA 高效传输 |
| 多线程 | 生产者-消费者模式 | **封装速度提升约 40%** |
| GOP 控制 | 适当增大 GOP (30-60帧) | 降低码率 |
| 码率控制 | CBR/VBR/QPMAP | 码流稳定性 |
| DMA | G2D 硬件加速图像处理 | 降低 CPU 占用 |

### 12.3 调试工具

```bash
# 查看 V4L2 设备
media-ctl -p
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video0 --all

# 查看 MPP 状态
cat /proc/mpp/
cat /sys/class/sunxi_info/sys_info

# 音频设备
arecord -l
aplay -l
cat /proc/asound/cards

# 帧率测试
echo 0x1 > /sys/module/sunxi_vin/parameters/vin_log

# ISP debug
echo 1 > /sys/module/sunxi_isp/parameters/isp_debug

# 性能分析
perf top
top -H
```

---

## 13. 现有源码对照表

以下是在 `/home/yyf/tina/第一期源码/` 和 `/home/yyf/tina/第二期源码/` 中已有的代码与本文档各模块的对应关系：

### 第一期源码 (视频基础)

| 现有代码路径 | 对应模块 | 功能 |
|-------------|---------|------|
| `05_全志多媒体平台基础/5-1_常见的多媒体框架/` | 模块 1-2 | MPP/GStreamer/FFmpeg Hello World |
| `06_视频输入输出模块/6-2_视频输入组件的使用/` | 模块三 VI | VI 组件基本使用 |
| `06_视频输入输出模块/6-4_获取摄像头数据/` | 模块三 VI | 从摄像头获取图像帧 |
| `06_视频输入输出模块/6-7_视频输出组件的使用/` | 模块四 VO | VO 组件基本使用 |
| `06_视频输入输出模块/6-8_绑定模式实现摄像头实时预览/` | 模块三+四 | VI→VO Bind 预览 |
| `06_视频输入输出模块/6-10_使用G2D实现图像旋转缩放/` | 模块九 G2D | G2D 旋转/缩放 |
| `06_视频输入输出模块/6-11_实时预览中加入旋转缩放功能/` | 模块九 G2D | VI→G2D→VO |
| `07_视频编码模块/7-4_编码原始YUV视频文件/` | 模块五 VENC | H.264 编码 YUV 文件 |
| `07_视频编码模块/7-6_编码摄像头视频流数据/` | 模块三+五 | VI→VENC 实时编码 |
| `07_视频编码模块/7-8_码流控制-QPMAP/` | 模块五 VENC | QPMAP 码流控制 |
| `07_视频编码模块/7-9_动态配置帧率和码率/` | 模块五 VENC | 动态参数调整 |
| `07_视频编码模块/7-10_GDC数字变焦功能/` | 模块五 VENC | GDC 数字变焦 |
| `08_RTSP流媒体协议/08-5_编写简单RTSP协议/` | 模块七 RTSP | 自实现 RTSP 服务器 |
| `08_RTSP流媒体协议/08-6_基于嵌入式Linux平台实现RTSP协议/` | 模块七 RTSP | 嵌入式 RTSP |
| `08_RTSP流媒体协议/08-7_使用Live555实现RTSP服务器/` | 模块七 RTSP | Live555 RTSP |
| `09_视频解码模块/09-2_使用ffmpeg解码H264文件/` | 模块六 VDEC | FFmpeg 软解码 |
| `09_视频解码模块/09-3_使用MPP平台解码H264文件/` | 模块六 VDEC | MPP 硬解码 |
| `10_视频叠加模块/10-2_使用ffmpeg实现叠加/` | 模块九 OSD | FFmpeg 叠加 |
| `10_视频叠加模块/10-4_实现位图数据的叠加/` | 模块九 OSD | 位图 OSD 叠加 |

### 第二期源码 (音频 + 封装 + USB)

| 现有代码路径 | 对应模块 | 功能 |
|-------------|---------|------|
| `11-3_音频文件格式/` | 模块七 AI | PCM/WAV/AAC 文件解析 |
| `14-2_使用FFMPEG封装音频和视频/` | 模块八 Muxer | FFmpeg 封装基础 |
| `14-4_视频流封装MP4文件/` | 模块八 Muxer | VI→VENC→MP4 |
| `14-5_音频流封装MP4文件/` | 模块七+八 | AI→AENC→MP4 音频封装 |
| `14-6_音视频流封装MP4文件/` | 模块七+八 | 完整 A/V→MP4 封装 |
| `14-9_MP4文件解封装音视频码流/` | 模块八 Demuxer | MP4 解封装 |
| `15-2_模拟摄像头UVC-Gadget驱动的使用/` | USB Gadget | UVC 虚拟摄像头 |
| `15-3_模拟USB摄像头-发送视频流数据/` | USB Gadget | UVC 数据发送 |
| `15-4_模拟USB摄像头和拾音器复合设备/` | USB Gadget | UVC+UAC 复合设备 |

---

## 14. 开发路线图

### 阶段一：环境搭建与验证 (1-2 天)
- [x] SDK 编译通过 (已完成)
- [ ] 烧录固件到开发板
- [ ] 验证基础外设 (串口、网络、USB)
- [ ] 验证摄像头设备节点 (`/dev/videoX`)
- [ ] 验证显示屏输出 (显示 Linux logo / 控制台)
- [ ] 验证音频设备 (MIC 录音, Lineout 播放)

### 阶段二：视频采集与显示 (2-3 天)
- [ ] 移植 `sample_virvi2vo` 实现摄像头预览
- [ ] ISP 参数调优 (色彩、曝光、白平衡)
- [ ] G2D 旋转/缩放集成
- [ ] OSD 位图叠加

### 阶段三：视频编解码 (2-3 天)
- [ ] H.264 硬件编码 (`sample_virvi2venc`)
- [ ] H.264 硬件解码 (`sample_vdec`)
- [ ] H.265 硬件编解码 (对比 H.264)
- [ ] 码率控制与动态参数调整

### 阶段四：音频采集播放与编解码 (2-3 天)
- [ ] ALSA 驱动对接验证
- [ ] AI 音频采集 PCM 数据
- [ ] AO 音频播放
- [ ] AAC 音频编码 (AENC)
- [ ] AAC 音频解码 (ADEC)
- [ ] 音频失真率测试与优化

### 阶段五：RTSP 流媒体 (2-3 天)
- [ ] 自实现 RTSP 服务器 (参考 `08-5`)
- [ ] 适配嵌入式平台
- [ ] 集成 Live555 (可选)
- [ ] 多客户端测试

### 阶段六：MP4 封装解封装 (2-3 天)
- [ ] 视频流 MP4 封装
- [ ] 音频流 MP4 封装
- [ ] 音视频同步 MP4 封装
- [ ] 多线程封装加速
- [ ] MP4 解封装实现

### 阶段七：系统集成与优化 (3-5 天)
- [ ] 完整数据流贯通 (Camera→Encode→RTSP+MP4)
- [ ] 音视频同步优化
- [ ] 性能调优 (帧率、码率、延迟)
- [ ] 长时间稳定性测试 (24h+)
- [ ] 文档输出与代码整理

---

## 附录 A：关键头文件索引

| 头文件 | 功能 |
|--------|------|
| `mm_comm_vi.h` | VI 公共定义 |
| `mpi_vi.h` | VI API 函数声明 |
| `mpi_sys.h` | MPP 系统 API (Init/Exit/Bind) |
| `mpi_venc.h` | VENC API |
| `mpi_vdec.h` | VDEC API |
| `mpi_ai.h` | AI API |
| `mpi_ao.h` | AO API |
| `mpi_aenc.h` | AENC API |
| `mpi_adec.h` | ADEC API |
| `mpi_isp.h` | ISP API |
| `mpi_g2d.h` | G2D API |
| `mpi_region.h` | OSD Region API |
| `mpi_videoformat_conversion.h` | 像素格式转换 |
| `mm_common.h` | 通用类型定义 |
| `vo/hwdisplay.h` | 硬件显示接口 |
| `confparser.h` | 配置文件解析 |

## 附录 B：参考资源

- V853 数据手册: `/home/yyf/tina/05_芯片手册/v853-amp-v853s_datasheet_v1.1.pdf`
- 开发板原理图: `/home/yyf/tina/04_开发板原理图/`
- 全志 Tina SDK 文档: `https://tina.100ask.net/`
- MPP 开发指南: 搜索 SDK 中的 PDF 文档
