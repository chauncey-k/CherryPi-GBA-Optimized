# Cherry Pi OOM 内存预算候选 · 2026-10-17

实验版，尚未在实板验证能否消除本次 OOM；不覆盖独立仓库的稳定发布程序。基于用户此前确认改善卡顿的 smc-live 程序，仅修改 Cherry Pi 的 ROM 缓存分配预算和失败处理。

## 日志证明了什么

本次日志明确出现：

```text
gpsp invoked oom-killer
Normal free:4072kB min:5020kB
Out of memory: Killed process 285 (gpsp) ... anon-rss:37908kB
Killed
No new profile: a game must run and exit normally ...
```

所以直接退出原因是 Linux 内存不足杀掉进程，不是已经证明的覆盖确认/音频互斥锁死锁。屏幕可能保留最后一帧，看起来像卡死。最后的 No new profile 是被杀后没有正常写出采样结果，不表示采样文件能解释另一个新故障。

日志当时系统可管理54804KiB、总物理64MiB、空闲4072KiB；CMA预留16MiB、其中当时空闲约2960KiB。CMA并不意味着整个16MiB永远不能用于普通可迁移页，不能简单把总RAM减16MiB当成精确用户内存预算。保留水位和分配限制下，日志显示系统已无法满足当时的请求。触发位置是文件映射缺页路径，不能从此栈声称 save_state 正好申请了某个大小的 malloc。

源码检查发现默认 ROM 匿名缓冲区32MiB；本款未压缩ROM也是32MiB，全部读入后占据大量常驻匿名内存。保存还涉及固定状态缓冲区、临时文件写入及文件缓存。其他翻译缓存、显示和内核也需要内存。仅靠 malloc 返回非NULL判断32MiB“够用”是不可靠的运行时预算。

## 修改

在 CHERRYPI 构建下将最大ROM缓存由32MiB改为24MiB。大于缓存的原始 .gba/.bin 文件使用已有32KiB分页机制；为本款32MiB ROM减少最多8MiB的匿名ROM缓存常驻量。没有承诺系统RSS一定精确下降8MiB，内核文件缓存及工作集也会变化。

分配失败按24/16/8/4MiB有限回退，页表分配失败会释放对应ROM缓冲区；全部失败则在SDL初始化前明确退出，避免原代码无界减法/未检查页表。

SMC动态音频模板优化、存档格式、校验、临时文件写完并fsync后rename、主菜单、驱动、DTB和内核都保留。它不是以取消存档安全检查换取成功，也没有改动 vm.min_free_kbytes、OOM策略或CMA大小。

代价：32MiB游戏会增加SD页面读取，某些场景可能比完全驻留略慢；仍需同场景比较帧数和声音。大于24MiB的 ZIP 内游戏不能走原始ROM分页，请先在电脑解压为 .gba；此候选会按原有容量检查拒绝过大的ZIP内容，不会写出缓冲区。

## 安装前备份

先正常退出/关机，保存原 `.svs` 和 `.sav` 到电脑或其他介质，不要反复覆盖唯一存档。原文件可能仍完整：原实现只有新内容写完才替换，但这份OOM日志不足以保证最后一次操作发生在替换前还是替换后。

把 cherrypi-memory-safe-20261017.tar.gz 复制到 GAMES 根目录，创建无扩展名空文件 DISABLE_GBA_AUTOSTART，上电串口登录 root。

板子执行：

```sh
mkdir -p /mnt/games
grep -qs ' /mnt/games ' /proc/mounts || mount -t vfat /dev/mmcblk0p3 /mnt/games
pidof gpsp cherrypi-launcher
```

应无进程输出，再执行：

```sh
gzip -dc /mnt/games/cherrypi-memory-safe-20261017.tar.gz | tar -xf - -C /root
sh /root/cherrypi-profile/install.sh
sh /root/cherrypi-profile/run-test.sh
```

安装器只接受原 smc-live 哈希 `923fb7fff27a3f0eb2bfc4b3599a62dd0149a478444ee8a521e49a563589f250` 或同一候选的重复安装。其他版本停止；不会覆盖已有个性化配置或存档。

安装前程序独立备份在 `/root/cherrypi-memory-safe-20261017/state/gpsp`。快速启动实验若已安装，其主菜单和BOOT文件不由此包改写。

## 验证

run-test.sh 不启用 CP_PROFILE；初始化已有声音通路，打开独立列表，后台每2秒记录一次 /proc 内存信息与gpSP RSS，最多300次，日志在tmpfs而非反复写SD。退出后自动停止监视并附加最近内核日志。同一串口在游戏运行时不能输入shell命令，先正常退出。

1. 确认启动打印 `ROM cache: 24 MiB (768 pages)`，使用未压缩 .gba。
2. 载入此前存档、进入同一场景；先在空槽位保存，再载入确认正常。
3. 对刚创建的测试槽位连续覆盖/加载几次，确认保留的数据正确，再观察原来会卡死的操作。
4. 运行数分钟，比较之前约60模拟帧时的声音/速度；不要只测试刚进入菜单几秒钟。
5. 正常退出到shell，执行：

```sh
cat /tmp/cherrypi-memory-test.txt
```

若仍出现OOM，保留完整日志；这意味着还要进一步测量内存峰值或工作集，不能把此候选当作已实板验证的最终修复。

测试通过再移除维护标记、sync、reboot，恢复上电列表。正常开机不启动该诊断监视器。

## 回退

退出游戏/列表、恢复维护模式，确认无两个进程后：

```sh
sh /root/cherrypi-profile/uninstall.sh
```

恢复安装前的smc-live程序，存档和设置保留。此前的稳定源码和发布包仍在独立仓库 components/gpsp 及 sdcard-files 中，没有被候选覆盖。

## 本地检查

真实 allocator/pager 源码通过 ASan/UBSan：合成32MiB ROM下2300次页面读取、淘汰后三个ROM镜像映射、RTC恢复、24MiB默认、ROM/页表分配失败与全部失败有限退出。原有存档错误注入、菜单覆盖确认、设置、按键、DMA216组和安装回退回归通过。实际ARM程序的音频动态分支1792组、存储243组通过；存档payload仍506951字节。完整对应源码、测试和GPL许可随包提供。

本地模拟不等于实板OOM复现；没有在用户板子上执行安装或覆盖个人存档。
