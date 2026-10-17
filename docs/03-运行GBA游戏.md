# 运行 GBA 游戏

## 上电免登录运行

游戏放在 SD 卡 `GAMES/roms`，推荐初次用英文文件名，例如 `my-game.gba`。Windows 的这个路径在板上对应 `/mnt/games/roms/my-game.gba`。

正常关机后取卡复制游戏，安全弹出再插回板子。确认 GAMES 根目录没有 `DISABLE_GBA_AUTOSTART`，正常上电进入主菜单。

方向键选 Games → A 或 Start 确认 → 选择游戏 → A 启动。最近游戏在 Recent games。游戏内 Menu 打开快捷菜单，选择退出游戏返回主菜单；“继续游戏”只关闭菜单，不退出游戏。

主菜单 Settings 调整音量和亮度，A/Start 保存，B取消。亮度要求背光硬件连通（本板之前已补焊 R44）。中文文件名显示暂不完善，推荐用英文名区分游戏。

## 从串口启动具体文件

GAMES 根目录有维护标记时，上电登录 root。执行：

```sh
mkdir -p /mnt/games
grep -qs ' /mnt/games ' /proc/mounts || mount -t vfat /dev/mmcblk0p3 /mnt/games
pidof gpsp cherrypi-launcher
```

查询应无进程输出，再设置声音并启动：

```sh
/root/audio-tools/audio-mode dsp
/root/audio-tools/amixer -c 0 sset 'Headphone Source' 'DAC'
/root/audio-tools/amixer -c 0 sset 'DAC' 100%
/root/audio-tools/amixer -c 0 sset 'Headphone' 25% unmute
/root/gpsp/run-gpsp.sh "/mnt/games/roms/my-game.gba"
```

完整路径加双引号，替换为实际文件名。不带参数 `/root/gpsp/run-gpsp.sh` 可打开模拟器自己的游戏列表。实体按键继续可用；键盘映射：WASD方向，J=A，K=B，U=L，I=R，Enter=Start，Space=Select，M=菜单，Q=退出。

想手动进入独立主菜单而非模拟器文件选择器：

```sh
SDL_VIDEODRIVER=dummy LD_LIBRARY_PATH=/root/gpsp/lib /usr/local/bin/cherrypi-launcher
```

游戏运行期间串口被程序占用，不能把 shell 命令贴进游戏里。先通过菜单或Q退出，再执行命令。

## 两种存档

- **游戏内存档 `.sav`**：在游戏自己的菜单保存进度。退出模拟器时让它正常写回，保留这个文件。
- **即时存档 `.svs`**：模拟器快捷菜单中的保存/读取槽位，是当时整个模拟状态。它不是游戏自己的存档，也不替代 `.sav`。

修改游戏文件名或换 ROM 版本可能影响对应存档路径和 ROM 校验。备份整个 `/root/gpsp`，同时保存 GAMES 中自行放置的 `.sav/.svs`；不要只复制 `.gba`。加载失败时保留原存档和报错，不要覆盖旧槽位试错。

## 关机与恢复登录

日常使用主菜单 Power off 并确认，等待系统停止后再断电。原型软件关机不保证硬件自动切断供电。

串口模式退出游戏回到 shell 后：

```sh
sync
poweroff
```

若主菜单异常而系统还能启动：在电脑给 GAMES 根目录加 `DISABLE_GBA_AUTOSTART`，重新上电进入串口维护。不要只依赖 Ctrl+C；某些运行状态不会响应。

## 常用检查

```sh
cat /proc/partitions
mount
ls -l /root/gpsp/gba_bios.bin
sha256sum /root/gpsp/gpsp
tail -n 80 /tmp/cherrypi-autostart.log
tail -n 80 /tmp/cherrypi-autostart.log.previous
cat /etc/cherrypi-autostart.conf
dmesg | tail -n 60
```

没有游戏先检查 `ROMDIR=/mnt/games/roms` 与实际路径。没有声音先查音频路由、dsp后端、音量和既有扬声器接线；OUT+/OUT−接扬声器两端。音频采样率不是无源扬声器本身的设置。游戏模拟速度不足仍可能使声音断续。
