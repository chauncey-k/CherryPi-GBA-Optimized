# Cherry Pi ROM 文件映射对照版（2026-10-17）

> 后续状态：该版本已在用户板上运行。收到存档后，精灵球交互 bad jump 已定位为特定 ROM 的无效文字指针，并经独立 ARM 指令验证；手动分页和 JIT 不是该错误的必要条件。当前组合与验证边界见 [当前版本文档](../../docs/07-当前版本与启动实测.md)。下文及归档内说明保留创建候选时的记录。

## 问题与证据

原先覆盖即时存档时的日志含 OOM，内核杀死 gpSP。24MiB 手动分页候选版随后出现另一问题：用户与同一精灵球交互时稳定退出，输出 `bad jump d1f8d1f8 (80f1862) (0)`。源码在打印 bad jump 后调用 quit，因此退出码 0 不代表游戏执行成功。

目前没有交互前的用户存档供本地复现；不能断言手动分页就是这次非法跳转的根因。此版本用于在同一事件对比，尚未在用户板上验证。

## 修改

对 32KiB 对齐、32KiB～32MiB 的原始 ROM 使用 `mmap(PROT_READ|PROT_WRITE, MAP_PRIVATE)`，文件以只读方式打开。ROM 的虚拟地址和内容连续，直接使用原有完整 ROM 映射路径，不再通过 24MiB 手动缓存换页。干净文件页可以由 Linux 回收；RTC 字节写入产生私有页，不改 SD 上的 ROM。

切换 ROM 时释放旧映射，并刷新六个 guest ROM 区域指针。映射失败报告错误，避免带着无效指针运行。小型非对齐文件和 ZIP 保持原有有界堆缓冲路径；32MiB 游戏请使用解压后的 `.gba`。

保留现有音频指令优化、存档格式、菜单及设备配置。文件映射仍会占用驻留内存；页缺失可能增加 SD 读取。不能保证所有事件兼容或绝不会 OOM。

## 安装和复测

1. 把已有 `.svs`、`.sav` 备份到电脑，保留精灵球交互前的存档。
2. 正常关机后，将 `cherrypi-rom-mmap-20261017.tar.gz` 复制到 GAMES 分区根目录。保持维护模式，登录串口后确认 `pidof gpsp cherrypi-launcher` 无输出。
3. GAMES 已挂载在 `/mnt/games` 时执行：

```sh
gzip -dc /mnt/games/cherrypi-rom-mmap-20261017.tar.gz | tar -xf - -C /root
sh /root/cherrypi-profile/install.sh
sh /root/cherrypi-profile/run-test.sh
```

安装器只接受已验证的 smc-live 正式基线、24MiB 候选版或本版本自身。首次备份放在 `/root/cherrypi-rom-mmap-20261017/state/gpsp`，重装不覆盖备份。

首次分配阶段仍可能打印 `ROM cache: 24 MiB`，之后加载 Equipo Rocket.gba 必须看到：

```text
ROM backend: private file mmap (32768 KiB); Linux page reclaim enabled
```

4. 加载同一交互前存档，与同一精灵球交互。先验证这一步，之后再在测试槽位验证保存、覆盖及加载，并玩 10～15 分钟观察声音和帧率。
5. 退出后读取 `cat /tmp/cherrypi-memory-test.txt`。同时保留串口的错误输出；该监控文件只记录内存与内核日志，不包含所有 gpSP stdout。仍退出时，需要交互前的 `.svs` 以复现；不要覆盖该文件。

### 回退

在 gpSP 与列表均退出后：

```sh
sh /root/cherrypi-profile/uninstall.sh
```

恢复本次安装前的二进制，可能是 24MiB 候选版；它仍可能在精灵球事件退出。安装不修改存档。

## 本地验证与重建

`tests/test_rom_mmap.py` 提取实际 C 加载代码，在 ASan/UBSan 下验证 32MiB 合成 ROM、六个镜像区、干净页丢弃后重读、RTC 私有写、映射失败、重复加载和堆缓冲切换。完整存档、菜单、设备设置回归保留。另用真实 ARM ELF 验证音频指令变体，并检查存档布局兼容。测试不包含 ROM、BIOS 或用户存档。

```sh
export CHERRYPI_SDK_HOST=/home/a/work/cherrypi/build/rootfs/host
bash build.sh
bash tests/run.sh
python3 tests/check_arm_layout.py
CHERRYPI_REPO_ROOT=/path/to/CherryPi-GBA-Optimized python3 package.py
bash tests/check_archive.sh
```

代码沿用 gpSP 上游 GPL 许可，完整源码与许可证在 `source/gpsp`。`fix.patch` 对比独立项目中的 smc-live 基线。`test_live_smc.py` 需要 pyelftools 和 Unicorn，另行安装至测试环境。
