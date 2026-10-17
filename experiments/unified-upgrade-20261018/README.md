# Cherry Pi 统一升级包（2026-10-18）

将已验证的 ROM-mmap gpSP、Midnight Mint 菜单及轻量动画、LCD 图形会话修复、设备控制和 fastboot stage1 r2 合并安装。不再需要先装静态主题再装动画。组件二进制保持原样，来源、对应源码与 SHA256 见 SOURCE_MAP.json；对应源码位于独立工程，链接固定到已有公开提交。

**适用于已经安装运行库、音频工具、GPIO 驱动和独立自动启动菜单的系统。不是新卡镜像，不安装内核、U-Boot、运行库、游戏或 BIOS。** 新卡仍需按仓库文档先安装基础镜像和 kit。现有 gpSP 必须为 smc-live、24MiB 内存候选或 ROM-mmap 的已知版本；菜单支持原独立菜单、fastboot r2、静态主题、封面候选和动画版。未知文件哈希拒绝覆盖，使用 status 输出诊断。

当前已装好动画的板子不需要为获得新功能再次安装；本包主要用于以后统一恢复和升级。组件已获此前板上反馈，新整合安装流程仅完成本机测试，仍需上板验收。

## 使用

把 `cherrypi-upgrade-20261018.tar.gz` 复制到 SD 卡 GAMES 分区根目录。退出游戏，主菜单处在串口按 Ctrl+C，登录 root，执行：

```sh
(
set -e
mkdir -p /mnt/games /mnt/boot
grep -qs ' /mnt/games ' /proc/mounts || mount -t vfat /dev/mmcblk0p3 /mnt/games
grep -qs ' /mnt/boot ' /proc/mounts || mount -t vfat /dev/mmcblk0p1 /mnt/boot
mount -o remount,rw /mnt/boot
if [ -d /mnt/games/DISABLE_GBA_AUTOSTART ]; then rmdir /mnt/games/DISABLE_GBA_AUTOSTART; fi
touch /mnt/games/DISABLE_GBA_AUTOSTART
gzip -dc /mnt/games/cherrypi-upgrade-20261018.tar.gz | tar -xf - -C /root
sh /root/cherrypi-upgrade-20261018/upgrade.sh install
rm /mnt/games/DISABLE_GBA_AUTOSTART
sync
reboot
)
```

只有出现 SUCCESS 后才移除维护标记并重启。失败时上述子 shell 会停止，标记保留。包与程序临时文件需系统分区至少 8 MiB 余量，BOOT 至少 128 KiB。安装器不自动删游戏、旧安装目录或旧回退备份。

## 安装范围和保留内容

替换八个文件：`/root/gpsp/gpsp`、`/usr/local/bin/cherrypi-launcher`、`/usr/local/sbin/cherrypi-autostart`、`cherrypi-display-session`、`cherrypi-device-control`、BOOT 的 `boot.scr` 和 `suniv-f1c100s-generic.dtb`，新增 `/etc/cherrypi-release` 版本记录。

保留 `/root/gpsp` 中其他文件、BIOS、全部 `.sav/.svs`、GAMES/roms、最近记录、`/etc/cherrypi-autostart.conf` 的 ROMDIR/音量/亮度。升级不会修复或替换用户游戏 ROM。日常启动仍由 supervisor 根据已有配置设置音频和背光。

## 备份、重装和整包回退

首次安装前八个目标的文件或缺失状态保存在 `/root/cherrypi-unified-20261018/first-backup`，SHA256 校验保护；重装保留首次备份。每次操作前另外创建 pending 快照，安装写入采用同目录临时文件和逐文件 rename。普通失败/INT/TERM 会尝试恢复本次操作前状态；恢复也失败则保留 pending 和维护标记，报告错误。安装会核对写入后的文件哈希。

整包回退需先恢复串口、挂载 GAMES 和 BOOT 为可写、创建维护标记，再运行：

```sh
sh /root/cherrypi-upgrade-20261018/upgrade.sh rollback
```

出现 SUCCESS 后移除标记，sync、reboot。回退到**首次统一升级前**的组合，不等于每个历史实验最初版本；首次统一升级前就已装动画的卡，回退仍为动画。若当前文件已被其他升级改动，回退会拒绝覆盖。

## 断电中断恢复

多个文件与两个分区不能实现一个硬件级原子事务。突然断电或 SIGKILL 无法执行自动回退；保持 GAMES 中的维护标记，重新上电后串口登录，挂载 BOOT 为可写，再执行：

```sh
sh /root/cherrypi-upgrade-20261018/upgrade.sh recover
```

recover 不依赖安装包 payload 校验，只验证 pending 备份并恢复本次操作前文件。恢复成功后可重试 install。若提示升级锁存在，先确认没有升级进程仍在运行，再 `rmdir /run/cherrypi-upgrade.lock`（重启会清空 /run）。备份创建阶段中断且未出现 APPLYING 时尚未替换目标；first-new 若不完整需先检查，再移走此暂存目录后重试。此机制不保证修复 SD 卡硬件损坏或断电造成的文件系统损坏。

## 状态与验收

`sh /root/cherrypi-upgrade-20261018/upgrade.sh status` 输出版本、八个目标的实际哈希及中断标记，不改系统。

上板后依次检查：启动进菜单、实体键选择、动画、进入/退出游戏、声音、亮度保存、游戏内存档与即时存档。第一次统一安装在这张已更新的卡上应保持组件哈希不变，主要新增版本记录和整包备份。回退后再次验收。

本机测试使用 BusyBox 和独立模拟文件树，覆盖前置检查、未知基线、包损坏、首次备份、重装、逐步写入失败、SIGKILL 恢复、回退失败、备份损坏及配置/存档/ROM 字节保留；不会在电脑上修改实际板卡。构建只需 Python3：`python make-package.py --repo <独立工程路径>`；测试 `python tests/test_upgrade.py`（Linux + BusyBox）。GPL-2.0-or-later，见 COPYING。
