# Cherry Pi 菜单轻量动画（2026-10-17）

在用户已上板确认正常的 Midnight Mint 静态主题上加入动画，不需要 PNG 素材。静态主题菜单基线 SHA256：`778493c890be55a283569e05619e65bc9c45ace36b4afef3ced2d3d527bba8c2`。

> 上板反馈：用户安装后确认“这个效果挺好的了”。动画视觉效果已获用户确认，长期运行负载仍需继续观察。

## 动画行为

- 页面切换：新页面内容横向轻移 8 像素，整数缓出，120 ms 结束；进入向左收拢，返回向右收拢，标题和底部提示立即更新。
- 切换选项：当前选中背景与侧边条用 100 ms 渐入；文字立即切换为选中状态，按键目标也立即改变。跨列表分页或循环时直接换到当前页，不滑过其他条目。
- 动画定时重绘间隔 40 ms，单次独立页面/高亮动画通常只有初始帧加 3 次重绘；完成后不再为动画重绘。按键和设置变化仍可立即重绘，不受动画帧间隔限速。
- 主循环继续处理事件，快速按键可重定向动画，不排队播放。启动游戏、确认关机和串口 Ctrl+C 均不等待动画结束。
- 首页启动首帧不播放入场动画；退出游戏后启动的新菜单也直接显示首页。

不改主题颜色、按键映射、模拟器、存档、系统启动或显示会话补丁。不含全屏淡入淡出、整屏截图缓存、浮点插值、定时线程或后台进程；新增动画状态只需几十字节。仍使用 320×240 逻辑画布，内容裁剪在面板范围内，动画不会覆盖页眉和底部提示。

`previews/menu-motion.gif` 是菜单代码生成的宿主测试帧演示，展示时间中额外加入了停留以便查看；不是板上录屏，也不能代表实际 LCD 刷新率。

## 安装

把 `cherrypi-menu-motion-20261017.tar.gz` 放到 SD 卡 GAMES 分区根目录。退出游戏，主菜单处用串口 Ctrl+C 回到 root 命令行：

```sh
(
set -e
mkdir -p /mnt/games
grep -qs ' /mnt/games ' /proc/mounts || mount -t vfat /dev/mmcblk0p3 /mnt/games
if [ -d /mnt/games/DISABLE_GBA_AUTOSTART ]; then rmdir /mnt/games/DISABLE_GBA_AUTOSTART; fi
touch /mnt/games/DISABLE_GBA_AUTOSTART
gzip -dc /mnt/games/cherrypi-menu-motion-20261017.tar.gz | tar -xf - -C /root
sh /root/cherrypi-menu-motion/install.sh
rm /mnt/games/DISABLE_GBA_AUTOSTART
sync
reboot
)
```

只接受已验证静态主题或本动画版本的哈希，不接受未知菜单。备份位于 `/root/cherrypi-menu-motion-20261017/state`，重装保留首次备份。只替换 `/usr/local/bin/cherrypi-launcher`，无需挂载 BOOT 分区。

## 实体板验收与回退

1. 开机首页直接可操作；上下切换时高亮短暂渐入。
2. 打开 Games/Recent/Settings、返回主页时内容有轻微位移，无闪黑。
3. 快速上下、反向切换、跨页后选中的名称始终对应当前游戏；长按左右时音量和亮度仍实时变化。
4. A/START 直接启动游戏；退出后回主页；Ctrl+C 仍可恢复串口命令行。

若板上出现明显拖影、闪烁或操作变慢，退出游戏并 Ctrl+C 后恢复已验证静态主题：

```sh
sh /root/cherrypi-menu-motion/uninstall.sh
sync
reboot
```

## 构建与验证

`source/motion.h` 实现整数动画进度和有限帧调度；`source/launcher.c` 在事件处理前后比较页面/选项，在原绘图路径上加位移和 RGB565 插值。没有全屏缓冲区复制。

本机检查包括 ARMv5 构建；动画时长、终点、反向/重定向、计时器回绕；真实菜单快速事件序列验证动画期间按键有效以及结束后停止 present；ASan/UBSan 检查绘制裁剪；原菜单设置、扫描、历史、启动路径和关机回归；BusyBox 模拟安装/重装/校验拒绝/回退；最终包解压后重跑测试。用户已确认实体板动画效果良好；长期负载和全部游戏场景未作统一保证。

设置 `CHERRYPI_SDK_HOST` 后 `bash build.sh`；`bash tests/run.sh` 运行宿主测试；`python make-previews.py` 用 Pillow 导出截图和 GIF；设置 `CHERRYPI_REPO_ROOT` 后 `python package.py` 打包。GPL-2.0-or-later。
