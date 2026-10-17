# Cherry Pi 快速启动第一阶段候选 r2 · 2026-10-17

> 后续状态：本版已在用户板上成功安装并生效。Linux 时间下菜单首帧记录为 3.56 秒；完整上电时间尚无秒表数据。启动日志对比和当前验证范围见 [当前版本文档](../../docs/07-当前版本与启动实测.md)。归档内说明保留上板前的候选记录。

r2 只更新安装兼容检查：接受已验证的 smc-live、24MiB 候选版及目前使用的 ROM 文件映射版 gpSP。不会替换 gpSP，也不修改修复后的游戏或存档。启动内容与上一版相同；如果已安装过第一阶段，这个 r2 不会再带来额外提速。

这是独立实验包，尚未上板测量，不代替已经验证的 GBA 性能优化发布版。用户当前反馈上电到主菜单约10～20秒。

## 本版做了什么

1. boot.scr 去掉 earlycon 和 ignore_loglevel，改为 loglevel=4；保留 ttyS0、SD根分区、rootwait、所有加载地址。
2. DTB 只把 SPI0/SPI NAND 与 Cedar 硬件视频解码的 status 改为 disabled；其他属性、时钟、phandle、显示、音频、按键、PWM、USB全部保持。
3. 主菜单首次显示前不扫描ROM目录，进入 Games 时才扫描。主菜单不再在未扫描时显示误导性的0游戏，而提示打开Games浏览。
4. 追加 RAM 中的时间记录 `/run/cherrypi-boot-timing.log`：launcher_start、menu_ready、rom_scan_begin/end。

它会减少候选开销；还没有实板证据说明能省几秒。按需扫描会把发现游戏的时间移到进入Games时，并没有让扫描本身变快。如果仅放少量ROM，扫描方面的收益可能很小。

本版不改 U-Boot，所以原有2秒倒计时仍在；不更换 zImage、不裁剪内核配置、不删除启动服务，也不替换已经提速的gpSP。下一阶段应根据实际启动日志决定是否裁剪内核、重编U-Boot零等待或调整服务顺序，不能盲目删除随机种子/挂载等初始化。

## 安装

1. 先正常关机。将 cherrypi-fastboot-stage1-r2-20261017.tar.gz 复制到 GAMES 根目录，并创建无扩展名空文件 DISABLE_GBA_AUTOSTART。
2. 插回上电，串口登录 root。以下在板子 Linux shell 执行：

```sh
mkdir -p /mnt/games /mnt/boot
grep -qs ' /mnt/games ' /proc/mounts || mount -t vfat /dev/mmcblk0p3 /mnt/games
grep -qs ' /mnt/boot ' /proc/mounts || mount -t vfat /dev/mmcblk0p1 /mnt/boot
mount -o remount,rw /mnt/boot
pidof gpsp cherrypi-launcher
```

进程查询必须无输出。之后执行：

```sh
gzip -dc /mnt/games/cherrypi-fastboot-stage1-r2-20261017.tar.gz | tar -xf - -C /root
sh /root/cherrypi-fastboot/install.sh
```

安装器验证当前 boot.scr、DTB、主菜单与优化gpSP的哈希。只接受此前已经验证的基线，或本候选自身用于重复安装。不匹配就停止，不应绕过校验。安装前备份位于 `/root/cherrypi-fastboot-stage1/state`，重复安装不覆盖首次备份。

看到 Installed 后：

```sh
rm -f /mnt/games/DISABLE_GBA_AUTOSTART
sync
reboot
```

重启测试一次后，再正常关机做冷启动测量。用手机秒表记录“按下供电→主菜单能响应按键”，连续测3次，并验证进入Games、游戏声音、按键、亮度、存档与退出回菜单。

## 看时间记录

在主菜单通过串口 Ctrl+C 恢复登录后（先退出游戏），执行：

```sh
cat /run/cherrypi-boot-timing.log
cat /proc/cmdline
dmesg
```

`menu_ready` 是首次成功呈现主菜单帧时的 Linux uptime，不包含 Boot ROM/SPL/U-Boot 和内核解压阶段，不等于完整上电时间。`menu_ready - launcher_start` 是菜单进程初始化到首帧的时间。`rom_scan_end - rom_scan_begin` 单独显示目录扫描耗时。每次重启菜单可能追加新记录，看本次启动最早一组。

日志只在RAM，重启会清空。若Ctrl+C未恢复登录，先记录手机实际秒数和现象，不要把下次维护模式启动的uptime当成上次自动启动时间。维护模式可以用于检查内核日志，但不会运行主菜单，因此不会有同样的menu_ready记录。

## 回退

进入维护模式，挂载BOOT读写、确认无gpsp/主菜单进程后：

```sh
sh /root/cherrypi-fastboot/uninstall.sh
rm -f /mnt/games/DISABLE_GBA_AUTOSTART
sync
reboot
```

恢复安装前的boot.scr、DTB和主菜单。gpSP优化版本始终保留。

若更换DTB后无法进入Linux：关机用读卡器打开BOOT，将本实验目录的 `boot-baseline.scr` 复制为 BOOT/boot.scr，将 `baseline.dtb` 复制为 BOOT/suniv-f1c100s-generic.dtb。不要重写整张卡。随后进入维护模式运行卸载脚本，恢复主菜单。

## 验证与源码

已完成 ARMv5 musl 编译、菜单行为测试（含首次启动不调用ROM扫描、进入Games仍扫描）、设置/按键/退出/关机确认回归。DTB结构对比确认只有两处status不同；启动脚本校验头部与数据CRC，确认除了调试参数外原内容保持。安装模拟测试覆盖错误基线与损坏包拒绝、安装、重复安装保留备份、回退及gpSP字节保持。

尚未实板测试速度和完整硬件行为。source、tests、launcher.patch、boot.cmd、baseline.dtb、boot-baseline.scr及GPL许可随包保存。build.sh需要 CHERRYPI_SDK_HOST（已有ARMv5 musl SDK）及 CHERRYPI_MKIMAGE（现有U-Boot mkimage路径），并依赖宿主fdtput。测试生成物在build目录，不随Git提交。
