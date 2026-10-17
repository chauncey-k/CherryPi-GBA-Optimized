# 游戏退出直接回菜单：LCD 图形会话修复候选（2026-10-17）

用户现象：正常退出游戏时，板载 LCD 短暂显示此前操作的一串按键字符，随后才显示主菜单。串口窗口不是本次问题的显示位置。

## 原因与修改

当前菜单通过 exec 替换为 gpSP，监护脚本待游戏退出后重新启动菜单。原有 framebuffer 适配器在每次进程退出时恢复其进入前的 KD 模式；当该模式是 KD_TEXT，就会重新显示 Linux 虚拟控制台的文本。GPIO 键同时经过 evdev 和 VT 键盘处理，前者供游戏读取，后者可能将按键写入控制台缓冲区。原监护脚本在游戏返回后还固定等待 1 秒。

本候选在整个自动启动会话期间持有 LCD 的 KD_GRAPHICS，并将该 VT 键盘模式改为 K_OFF。菜单和游戏因此保存/恢复的都是图形模式，进程切换间不切回文字控制台。K_OFF 不对 /dev/input/event* 使用 EVIOCGRAB，也不改 ttyS0 的 termios，游戏的 evdev 和串口输入处理保持原状。工具不常驻，没有增加一个持续运行的菜单进程。

监护脚本在正常/异常游戏退出后立即回到菜单；菜单自身反复启动失败时仍保留重试间隔。Ctrl+C 暂停自动启动、维护标记、关机和脚本正常退出时恢复保存的 VT 模式与键盘模式。重复 acquire 不覆盖首次保存的模式，未正常退出后再次启动可沿用原始恢复记录。

helper 仅操作 /dev/tty0 确认的实际 LCD VT（通常 tty1），清理该 VT 的旧文本缓冲及输入队列；不会清理串口日志。动态 VT 切换、外部其他图形程序争用和强制断电不属于已经实测的范围。

## 安装

需要已工作的独立主菜单与自动启动监护脚本。与当前 ROM 文件映射版、fastboot stage1 r2 配合，不替换 gpSP、主菜单二进制、boot.scr、DTB、内核、设置或存档。

1. 正常关机后，将 `cherrypi-display-session-20261017.tar.gz` 复制到 GAMES 根目录。
2. 上电后先退出游戏，主菜单按串口 Ctrl+C 恢复登录。确认 `pidof gpsp cherrypi-launcher` 无输出。
3. 在板上执行以下完整块；任意步骤失败即停止，不继续重启：

```sh
(
set -e
mkdir -p /mnt/games
grep -qs ' /mnt/games ' /proc/mounts || mount -t vfat /dev/mmcblk0p3 /mnt/games
touch /mnt/games/DISABLE_GBA_AUTOSTART
gzip -dc /mnt/games/cherrypi-display-session-20261017.tar.gz | tar -xf - -C /root
sh /root/cherrypi-display-session/install.sh
rm /mnt/games/DISABLE_GBA_AUTOSTART
sync
reboot
)
```

维护标记必须是普通文件，同名文件夹会被拒绝。安装器先校验包和原监护脚本，备份在 `/root/cherrypi-display-session-20261017/state`。重复安装不覆盖首次备份。若安装被拒绝，保留错误输出，不绕过哈希检查。

## 上板验证

使用正常上电自动进入菜单的流程，用实体按键玩一段时间，再从游戏菜单正常退出。预期直接回到主菜单，不显示按键字符；最多有图形帧切换，不保证逐帧无黑屏。反复测试进入/退出、声音、亮度、存档和按键。

再在主菜单按串口 Ctrl+C：应恢复登录，此时执行：

```sh
/usr/local/sbin/cherrypi-display-session status
tail -n 60 /tmp/cherrypi-autostart.log
```

维护状态预期 display=text、keyboard=enabled、saved_session=no（取决于原始 VT 模式）。应用运行期间工具会记录 session acquire，但进入维护后会恢复。`run-test.sh` 是串口诊断入口，不是此自动返回菜单会话的替代测试。

## 回退

退出应用并通过 Ctrl+C/维护标记恢复登录后：

```sh
sh /root/cherrypi-display-session/uninstall.sh
sync
reboot
```

恢复原监护脚本和原 helper（如有），保留游戏及全部存档。若屏幕状态异常，串口登录后也可先手动运行 `/usr/local/sbin/cherrypi-display-session release` 恢复 LCD；不要删除 `/run/cherrypi-display-session.state` 后再尝试恢复。

## 本地验证与构建

已完成 ARMv5 编译。实际 C 状态逻辑在 ASan/UBSan 下模拟 tty ioctl，覆盖图形模式保持、K_OFF、重复 acquire/release、模式切换失败回滚、恢复失败重试、损坏/符号链接状态拒绝；测试不访问真实 VT。实际 BusyBox 监护脚本覆盖游戏返回、游戏错误、SIGINT、反复崩溃、关机、维护标记、helper 失败和缺失。

真实板卡的 LCD 视觉过渡和 GPIO 行为仍待用户验证，不把模拟 ioctl 测试等同于真机结果。

```sh
CHERRYPI_SDK_HOST=/home/a/work/cherrypi/build/rootfs/host bash build.sh
bash tests/run.sh
CHERRYPI_REPO_ROOT=/path/to/CherryPi-GBA-Optimized python3 package.py
```

工具、监护脚本和测试为 GPL-2.0-or-later，见 COPYING。该目录是候选补丁，不自动更新到已上传的当前版本组合。
