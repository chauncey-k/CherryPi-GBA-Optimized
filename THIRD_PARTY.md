# 来源与许可

gpSP 基于 Apaczer/gpsp 的 miyoo 分支，固定提交见 sources.lock.json；对应修改源码在 components/gpsp/source/gpsp，GPL-2.0-or-later，保留各文件版权及 COPYING。fix.patch 是相对上一轮 smc-guards 的性能改动，完整源码才是本版的完整基线。

独立菜单、设备控制及原有运行脚本来自本项目此前已验证实现，对应 source、tests 和脚本一并保存。菜单随包许可为 GPL-2.0-or-later。

SD 卡安装包保留 SDL、libpng、zlib、ALSA 的原许可文件。audio-tools 源码归档与 Buildroot 构建配置保存在 components/audio-tools/source。SDL/libpng/zlib 对应归档及 Buildroot 包配方、补丁另存在 components/runtime-libraries。上游系统版本、固定提交和下载校验见 sources.lock.json；系统补丁、配置和板级文件在 components/linux-system。

本仓库是 Cherry Pi 原型的软件整理版，不是最初 PCB 开源工程的替代版本。基础 Linux 5.4.196、U-Boot 2022.01 来自此板实际使用的社区分支；不要误套最初工程 README 的其他内核/U-Boot 版本。

不附带 GBA BIOS、商业游戏、个人存档或整卡个人备份。firmware 中的早期基础镜像仅为本地恢复副本，由 .gitignore 排除；若以后发布系统镜像或第三方库二进制 Release，应同时提供与实际构建匹配的完整对应源码、配置、补丁和许可，不能只上传镜像。当前未创建或上传 GitHub Release。
