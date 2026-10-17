# Cherry Pi 游戏封面预览（2026-10-17）

基于已验证的 fastboot stage1 r2 菜单。左侧八行列表，右侧静态封面；Games、Recent games 均可预览。首页不扫描 ROM、不读取封面。设置、实体按键、快速启动计时和退出返回菜单行为保留。

本包只替换 `/usr/local/bin/cherrypi-launcher`，不修改 gpSP、存档、启动脚本、内核、设备树或显示会话补丁。已有显示会话补丁可以继续使用。仅接受 fastboot r2 菜单及本版自身的 SHA256；未知版本拒绝覆盖。依赖现有 `/root/gpsp/lib/libpng16.so.16` 和 `libz.so.1`。

## 封面文件

SD 卡在 Windows 显示的 GAMES 分区：

```text
roms/Equipo Rocket.gba
covers/Equipo Rocket.png
roms/Equipo Rocket - repaired.gba
covers/Equipo Rocket - repaired.png
```

文件名与 ROM 去掉扩展名后的名称一致，包括空格。PNG 后缀使用小写 `.png`。也支持把图放在 ROM 所在文件夹的 `covers` 子目录，优先于 GAMES 根目录的 `covers`，适合区分不同子目录中的同名游戏。Recent games 使用同一查找规则。添加或更换图片后，切换到另一个游戏再切回来即可刷新。

PNG 最大 512×512 像素、512 KiB，建议 224×288 以内。支持 RGB、RGBA、灰度、调色板、16 位和交错 PNG，保持比例，透明处使用菜单背景。缺失、超限、损坏时显示占位图，游戏仍能启动。板端不直接读取 JPG。

可在电脑用 Pillow 把自己的 JPG/PNG 转为合适尺寸，输出文件不能已存在：

```powershell
python -m pip install Pillow
python prepare-cover.py "C:\Pictures\cover.jpg" "F:\covers\Equipo Rocket.png"
```

不附带商业游戏封面、ROM 或 BIOS。界面中的卡带占位图由代码绘制。

## 安装

将 `cherrypi-cover-preview-20261017.tar.gz` 复制到 GAMES 分区根目录。板上退出游戏，菜单处按串口 Ctrl+C 恢复 root 命令行。执行：

```sh
(
set -e
mkdir -p /mnt/games
grep -qs ' /mnt/games ' /proc/mounts || mount -t vfat /dev/mmcblk0p3 /mnt/games
if [ -d /mnt/games/DISABLE_GBA_AUTOSTART ]; then rmdir /mnt/games/DISABLE_GBA_AUTOSTART; fi
touch /mnt/games/DISABLE_GBA_AUTOSTART
gzip -dc /mnt/games/cherrypi-cover-preview-20261017.tar.gz | tar -xf - -C /root
sh /root/cherrypi-cover-preview/install.sh
rm /mnt/games/DISABLE_GBA_AUTOSTART
sync
reboot
)
```

失败时维护标记保留，先查看报错。备份保存在 `/root/cherrypi-cover-preview-20261017/state`，重复安装不会覆盖首次备份。不需要挂载 BOOT 分区。

## 上板验收与回退

1. 首页启动时间与原来相近；进入 Games 后选中游戏，约 180 ms 稳定后显示封面。
2. 连续按上下键后停下，封面必须对应当前游戏；缺图和坏图显示占位图，不能卡住。
3. Recent games 对应同一封面；调整亮度/音量后仍能立即刷新并保存。
4. 启动游戏、存取原存档、退出回菜单；之前的显示会话补丁仍能隐藏控制台字符。

需要回退时退出游戏、Ctrl+C 后执行：

```sh
sh /root/cherrypi-cover-preview/uninstall.sh
sync
reboot
```

## 资源和验证

仅保存当前选中游戏的一张 RGB565 缩略图，固定 32,256 字节。连续切换会推迟解码；同一选择包括缺失/损坏结果不重复读盘。切到首页/设置即清除选择。解码时临时 RGBA 最多 1 MiB，libpng 自定义分配总额上限 2 MiB，输入文件上限 512 KiB、单个辅助块上限 64 KiB；解码完成释放临时内存。启动游戏使用 exec，菜单缓存随进程映像替换释放。

本机验证：ARMv5 编译；真实 libpng 解码、尺寸/透明/坏图/延迟/缓存测试；原菜单设置、历史、惰性扫描和精确游戏路径执行测试；安装、重装、校验拒绝、回退和其他文件不变测试。安装前仍需按上面步骤在实体板验收，尚未宣称已上板通过。

构建：设置 `CHERRYPI_SDK_HOST` 后运行 `bash build.sh`；`bash tests/run.sh` 运行宿主测试（需要 gcc、系统 libpng16、Python3、BusyBox）；打包设置 `CHERRYPI_REPO_ROOT` 后运行 `python package.py`。测试使用 SDK 的 libpng 公共头文件和宿主 libpng16 ABI。GPL-2.0-or-later，见 COPYING。
