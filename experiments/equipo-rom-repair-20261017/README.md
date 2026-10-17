# Equipo Rocket 背包分类指针修复实验（2026-10-17）

本目录只包含修复工具与说明，不含 ROM、BIOS、存档或截图。生成的游戏文件、存档和安装包必须放在独立项目的 `local/`，不提交 GitHub。

## 修复依据

当前 ROM SHA1 为 `60b3c1b3d5b94d0a1ec2f2662c55b86d9a259d9a`。交互脚本按背包类别分支：类别 6 读取标准字符串第 30 项，类别 7 读取第 31 项。两个表项是无效地址，但 ROM 内仍有语义对应且以 0xff 结束的现存文本。

| 文件偏移 | 原字节 | 修复字节 | 目标文本 |
| --- | --- | --- | --- |
| 0x00c4c4fc | 4a 4b 4c 4d | 41 9a cb 08 | 0x08cb9a41: MEDICINAS（药品） |
| 0x00c4c500 | 4e 4f 00 00 | 4b 9a cb 08 | 0x08cb9a4b: OBJ. BATALLA（战斗道具） |

修复仅改 8 字节，输出新的 `Equipo Rocket - repaired.gba`。SHA1：`5ad0baa726e15414738b9597caebdb80d7373ef8`。这恢复了分类词的语义对应关系，不是作者原始 ROM 的逐字节历史还原。当前游戏还有自定义中文文本，这两个分类词仍使用 ROM 里现有的西班牙语；不声称完成整份游戏汉化或所有 ROM 数据修复。

## 已做验证与范围

- 两份用户存档的头/载荷 CRC、ARM 布局和 ROM 身份正确。
- 原故障在 headless gpSP ARM ELF 回放第 67 帧复现；真实游戏字符串命令把无效地址传给 StringCopy，持续覆盖 EWRAM 回调，最终 bad jump。
- 独立 ARM926 直接执行原始 guest 指令也得到相同错误指针，不需要 gpSP JIT 或手动分页。
- 修正后，同一交互回放至 280 帧，无 bad jump，领取提示、精灵球消失、对话关闭。headless 宿主/BIOS有近似，仍需真机复测。
- 最终生成的修复 ROM 文件与转换存档再次配对回放，关闭临时内存修补，实际运行到 300 帧且无 bad jump。新存档的头/载荷 CRC、schema 和新 ROM SHA1 均通过。
- 两个类别分别通过实际 guest ARM 指令调用/字符串复制，能正常返回，且目的文本缓冲区之外 EWRAM 字节不变。类别 7 是受控指令测试，尚未完成实际战斗道具拾取事件测试。
- 新即时存档仅更新 ROM 身份、512 字节文件名字段和 CRC，CPU/RAM 等游戏状态不变。实际 `state_io.c` 校验保持开启：旧存档对新 ROM 被拒绝，转换后的存档被接受。
- 尚未确认真机交互、背包数量、游戏内保存重读和长时间运行；不能把离线成功等同于全部验证完成。

## 自行生成私有测试文件

需要 Python 3，无第三方包。工具严格匹配已核验的原始 ROM 与交互前存档 SHA，其他输入拒绝修补。

```powershell
python experiments/equipo-rom-repair-20261017/repair.py --rom "原ROM路径/Equipo Rocket.gba" --state "交互前存档路径/Equipo-Rocket-before-interaction.svs" --out local/equipo-repair-20261017
python experiments/equipo-rom-repair-20261017/make-sd-package.py local/equipo-repair-20261017
```

输出文件夹必须不存在。原始文件只读，不覆盖。转换的存档是特定 8 字节修复 ROM 的测试副本，不能用本工具强行绑定任意其他 ROM。

## 上板测试

先备份原文件，正常关机再取卡。复制以下两项：

1. `Equipo Rocket - repaired.gba` → GAMES 分区 `roms/`。
2. `cherrypi-equipo-romfix-20261017.tar.gz` → GAMES 分区根目录。该包包含私人存档，不上传 GitHub。

保持维护模式。串口登录后确认 `pidof gpsp cherrypi-launcher` 无输出。GAMES 已挂载到 `/mnt/games` 后运行：

```sh
gzip -dc /mnt/games/cherrypi-equipo-romfix-20261017.tar.gz | tar -xf - -C /root
sh /root/equipo-romfix-20261017/install.sh
sh /root/cherrypi-profile/run-test.sh
```

选择 **Equipo Rocket - repaired**，加载即时存档 **Slot 0**，重复精灵球交互。这份检查流程使用前一版 ROM 文件映射版 gpSP；安装包不替换模拟器、内核、菜单或音量配置。

安装器校验 ROM 与存档，给修复版单独安装 Slot 0；存在不同内容的该槽位时会拒绝覆盖。若 `/root/gpsp/Equipo Rocket.sav` 存在且新游戏同名 `.sav` 不存在，会复制游戏内存档供新副本使用。原 ROM 和原即时存档保留。

验证领取后道具数量增加、放入正确分类、精灵球消失；再对测试游戏执行即时保存/加载、游戏内保存和重新启动读取。退出后保存串口输出与 `/tmp/cherrypi-memory-test.txt`。

回退时在游戏列表选择原来的 **Equipo Rocket**。不要删修复版的新存档，直到确认不需要其中的新进度。

## 公开源码参考

解释指令时参考 pret/pokeemerald 的 [脚本命令表](https://github.com/pret/pokeemerald/blob/master/data/script_cmd_table.inc)（0x48 checkitemtype、0x84 bufferstdstring）、[ScrCmd_bufferstdstring](https://github.com/pret/pokeemerald/blob/master/src/scrcmd.c) 与 [字符映射](https://github.com/pret/pokeemerald/blob/master/charmap.txt)。原版地址不直接套用到改版 ROM。

修复工具为 GPL-2.0-or-later；ROM 与用户存档不属于本工具的发布内容。
