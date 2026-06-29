# SDK 修改记录

## 编译兼容性修复 (Ubuntu 20.04)

### 1. libubox json-c 头文件路径修复
- **文件**: `openwrt/openwrt/package/libs/libubox/Makefile`
- **修改**: 添加 `-I$(STAGING_DIR)/usr/include/json-c` 到 TARGET_CFLAGS
- **原因**: json-c 0.15 将头文件移到 `json-c/` 子目录
- **命令**: `git add -f openwrt/openwrt/package/libs/libubox/Makefile`

### 2. host libubox json-c 头文件软链接
- **位置**: `openwrt/openwrt/staging_dir/host/include/`
- **操作**: 创建 json-c 所有 .h 文件的软链接到上层目录
- **原因**: cmake 中文路径编码问题导致 include path 损坏

## 如何记录新的 SDK 修改

当你修改 SDK 中某文件后:
1. `git add -f <文件路径>` 强制跟踪该文件
2. 在本文档中记录修改内容和原因
3. `git commit -m "fix(kernel): xxx"` 提交
