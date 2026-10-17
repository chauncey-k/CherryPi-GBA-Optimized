# Cherry Pi 静态主题：Midnight Mint（2026-10-17）

基于 fastboot stage1 r2 的独立菜单，先统一静态视觉，等待用户确认后再加入轻量动画。无需 PNG、字体文件或其他素材。游戏菜单仍使用原有 320×240 逻辑画布，居中显示在 480×272 LCD 上。

本目录 `previews/overview.png` 是真实菜单渲染的四页拼图（宿主测试帧，非实体板照片）。游戏名称来自测试数据，不包含 ROM。

## 界面变化

- 深蓝背景、分层面板、薄荷绿选中竖条和浅色文字；警告和关机确认使用琥珀色。
- 首页四张卡片，分别显示功能名称和一句说明；列表有固定边距和八行分页。
- 游戏列表全宽，去掉显示名称中的 `.gba`/`.zip`/`.bin` 后缀；超长名称以省略号结尾，执行时仍传完整原始路径。
- 保留已有 6×10 位图字体，主文字字符间距由 6 改为 7 像素，辅助文字使用 6 像素间距；尚未添加中文字体。
- 音量和亮度显示右对齐百分比与进度条，保持实时预览、长按调整和保存/取消。
- 底部使用按键标识，随页面显示 Move/Open/Play/Back/Settings 或 Adjust/Save/Cancel；A 对应实体 A，M 对应现有 Menu 键，START 也能打开/播放。
- 无游戏、无最近记录、设置失败、关机确认均使用同一套配色和间距。

此版本不包含切换动画，也不读取封面。此前封面候选包继续单独保留；若已安装它，安装本主题会改回全宽列表，SD 卡图片不会删除。

## 安装

将 `cherrypi-theme-20261017.tar.gz` 复制到 SD 的 GAMES 分区根目录。退出游戏，在主菜单处用串口 Ctrl+C 恢复 root 命令行，然后执行：

```sh
(
set -e
mkdir -p /mnt/games
grep -qs ' /mnt/games ' /proc/mounts || mount -t vfat /dev/mmcblk0p3 /mnt/games
if [ -d /mnt/games/DISABLE_GBA_AUTOSTART ]; then rmdir /mnt/games/DISABLE_GBA_AUTOSTART; fi
touch /mnt/games/DISABLE_GBA_AUTOSTART
gzip -dc /mnt/games/cherrypi-theme-20261017.tar.gz | tar -xf - -C /root
sh /root/cherrypi-theme/install.sh
rm /mnt/games/DISABLE_GBA_AUTOSTART
sync
reboot
)
```

只替换 `/usr/local/bin/cherrypi-launcher`。接受已验证的 fastboot r2、上一版封面候选菜单和本主题自身的 SHA256；其他版本拒绝覆盖。首次备份保存在 `/root/cherrypi-theme-20261017/state`，重装不覆盖首次备份。已有显示会话补丁、快速启动脚本、内核、gpSP、ROM 和存档不修改。不需要安装封面版，不需要挂载 BOOT 分区。

## 上板验收和回退

1. 首页首帧与原来相比无明显延迟，实体上下键切换四张卡片。
2. Games/Recent games 中看清选中条；长按上下、跨页和循环选择正常；A 或 START 能启动正确游戏。
3. 左右键修改音量/亮度，百分比和进度条立即刷新；A 保存、B 取消。
4. 退出游戏返回主题菜单，串口 Ctrl+C 仍能恢复命令行；关机必须再次确认。

退出游戏并 Ctrl+C 后，可恢复安装前菜单：

```sh
sh /root/cherrypi-theme/uninstall.sh
sync
reboot
```

## 工程与验证范围

调色板位于 `source/theme.h`，排版位于 `source/launcher.c` 的 render/footer/tile/text 函数。仅在 dirty 时绘制，空闲时不连续刷新，无动画定时器、图片解码、额外依赖或驻留进程。ROM 扫描仍推迟到进入 Games，保留 `/run/cherrypi-boot-timing.log` 中的 launcher_start/menu_ready/rom_scan_begin/end。

已完成 ARMv5 构建和宿主菜单回归测试，包括设置即时预览、保存失败/取消、方向键长按、关机确认、原始游戏路径传参、惰性扫描；检查绘制区域和空列表、八行分页、长标题、失败提示等截图；BusyBox 模拟安装/重装/未知基线拒绝/校验拒绝/维护标记/回退及其他文件保留。最终压缩包解压后重新运行相同测试。尚未在实体板验收，未声明完整上电时间或所有游戏性能不变。

构建：设置 `CHERRYPI_SDK_HOST` 后运行 `bash build.sh`。测试：`bash tests/run.sh`（gcc、ASan/UBSan、Python3、BusyBox）；打包：设置 `CHERRYPI_REPO_ROOT` 后 `python package.py`。GPL-2.0-or-later，见 COPYING。
