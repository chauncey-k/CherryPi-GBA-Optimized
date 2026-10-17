# 构建、测试与 GitHub

## 本地仓库与提交范围

这是独立目录，可在 `C:\learn\CherryPi-GBA-Optimized` 建立自己的 `.git`。不复用原 PCB 工程的 Git 历史或远程地址。本次没有执行 commit、创建远程仓库或 push。

在本项目根目录 PowerShell 先校验：

```powershell
python .\tools\verify.py
git status --short
git check-ignore firmware/sdcard-lcdtest-20261009.img.gz
```

镜像、ROM、BIOS、存档、build、日志和本地测试素材被忽略。安装 tar.gz 会提交，对其内部也已检查并剔除 BIOS、游戏和个人存档。不要使用 `git add -f` 强行加入这些排除项。

准备首次提交时：

```powershell
git add README.md VERSION COPYING THIRD_PARTY.md .gitignore .gitattributes sources.lock.json RELEASE_MANIFEST.json LICENSES docs tools components sdcard-files firmware/README.md firmware/image.json
git diff --cached --stat
git diff --cached --name-only
git commit -m "Add verified Cherry Pi GBA optimized release"
```

如果 Git 要求身份信息，使用自己的姓名和邮箱配置。之后自行在 GitHub 创建空仓库，复制真实远程地址：

```powershell
git remote add origin <GitHub仓库的真实URL>
git push -u origin main
```

上面的占位符先替换，不能原样执行。镜像未纳入 Git，README 应明确克隆者需取得基础镜像或自行构建。若之后要发布系统镜像二进制，先补齐与所发布二进制匹配的对应源码材料和许可，见 THIRD_PARTY.md。

## 重编译 gpSP

使用 Linux/WSL 和可用的 ARMv5 musl Buildroot SDK；需包含 SDL 1.2、libpng、zlib。仅有 `arm-linux-gnueabi-gcc` 不足以链接本程序。

在 WSL 进入本项目路径，然后：

```sh
export CHERRYPI_SDK_HOST=/absolute/path/to/buildroot/output/host
bash components/gpsp/build.sh
```

`CHERRYPI_SDK_HOST` 指向实际 SDK 的 host 目录。当前电脑已有的已验证 SDK 位于 `/home/a/work/cherrypi/build/rootfs/host`；这是外部编译环境，不是对原 Windows 工程目录的依赖。

新 ELF 在 `components/gpsp/build/gpsp`，剥离程序和符号表在该 build 目录。默认不覆盖发布目录中的已验证二进制。若修改了代码，测试通过后才用 `build.sh --stage` 覆盖候选发布程序，再运行：

```sh
python3 tools/package-optimized.py
```

这会重新打包 optimized tar.gz 并更新其及 kit 的校验和；同时需要用工具更新根 RELEASE_MANIFEST.json。版本号、程序哈希和性能文档也要随正式新版本更新，不能把修改后的程序继续宣称为已实机验证的旧哈希。

## 测试

普通宿主回归需要 Linux gcc、Python3、BusyBox：

```sh
bash components/gpsp/tests/run.sh
python3 components/gpsp/tests/test_quick_install.py
bash components/gpsp/tests/check_archive.sh
```

执行真实 ARM ELF 语义检查需宿主 Python 的 Unicorn、pyelftools（不安装到板上）：

```sh
python3 -m pip install unicorn==2.1.4 pyelftools==0.33
python3 components/gpsp/tests/test_live_smc.py
python3 components/gpsp/tests/test_arm_smc.py
python3 components/gpsp/tests/test_guard_flush.py components/gpsp/build/gpsp --expect-clean
```

完整混音循环部分需要用户自行从存档提取的32KiB IWRAM，默认没有则明确 SKIP；其余合成指令测试照常执行。可设置 `CP_IWRAM_FIXTURE=/path/to/iwram.bin` 启用该部分。此素材不提交仓库。原始验证曾覆盖这一部分，见性能记录。

历史对照 ELF 不随 Git 分发；test_arm_smc.py 默认只测当前版本，也可显式传入当前和旧 ELF 进行比较。

主菜单可用 `CHERRYPI_SDK_HOST=... bash components/main-menu/build.sh` 重编译；其 source/tests 独立保存在对应组件目录。系统监护与设备控制脚本在 components/system-scripts。

## Linux / U-Boot / 基础镜像构建资料

`components/linux-system` 保存此前实际工程的 configs、patches、board、scripts 和来源锁定记录，不包含庞大的社区源码树。先按 sources.lock.json 获取固定提交的 sunxi_kernel、sunxi_uboot、Buildroot，分别放入该目录同名位置；Buildroot 两个 WSL 补丁见 patches。内核/U-Boot 使用 `arm-linux-gnueabi-`，用户态使用 Buildroot 内的 ARMv5 musl SDK。

脚本从自己的目录解析根路径；导出的配置中的 `@SYSTEM_ROOT@` 会在 rootfs 构建脚本中替换成本目录路径。完整内核和 rootfs 的已用配置另存为 kernel-verified.config、rootfs-verified.config。Buildroot 保存的 toolchain/包缓存需重新取得，不能假定只复制配置就有完整 SDK。

```sh
cd components/linux-system
bash scripts/build-uboot.sh
bash scripts/build-kernel.sh
bash scripts/build-rootfs.sh
bash scripts/make-sdimage.sh
python3 scripts/verify-sdimage.py
```

镜像配方默认产生基础系统；菜单、BIOS和优化程序仍按安装文档部署。上述完整系统重构本次没有重新联网拉取及全量编译验证；本地现成恢复镜像的内容和哈希固定，不承诺重构结果逐字节相同。
