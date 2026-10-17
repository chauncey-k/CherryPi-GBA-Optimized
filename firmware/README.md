# 本地基础镜像

本机整理目录中保存 sdcard-lcdtest-20261009.img.gz，来自此前验证的321MiB基础镜像。SHA256、原始大小及压缩文件校验见 image.json。

它是早期两分区基础系统；写卡后必须继续安装 sdcard-files/cherrypi-kit 的内核与用户程序，才有现在的按键、主菜单和优化版本。不是用户32GB卡的完整状态，不能恢复个人存档。

`.img` 和 `.img.gz` 被 Git 忽略，因此 GitHub 克隆不会取得镜像。取得对应镜像后放在本目录，执行根目录 tools/unpack-image.py；或根据 components/linux-system 的固定来源、配置和补丁自行重构基础系统。不要以零字节或未读完的卡镜像替代。

此目录用于本地恢复。向外发布完整系统二进制 Release 前需一并整理实际对应源码及许可材料；本次未执行发布。
