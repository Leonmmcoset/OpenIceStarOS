# OpenIceStarOS x86 反编译与源码恢复

原始磁盘为 `IceStarOS_x86_32M.vhd`。FAT16 文件已提取到本地的
`IceStarOS_x86_32M_extracted/`；提取结果和构建产物由 Git 忽略。
所有测试镜像均写入 `build/` 或 `rewrite/build/`，运行时使用 QEMU
快照。原始 VHD 保留作为反编译和差分验证的参考。

**完整操作系统尚未恢复。** 当前可从源码编译三个引导阶段，第三阶段
已移除对原始引导机器码的 `.incbin` 引用。INIT 和内核仍为部分恢复；
内核有 352 个已安装的源码替换区间。已恢复并编译五个应用：SNAKE、
COLOR、MOV、STARSCR、CLOCK。原盘共 39 个 `.X`/`.DX` 路径、37 个不同
程序，仍包含未恢复的驱动和应用。区间数量不能当作完成比例。

| 内容 | 源码与说明 |
| --- | --- |
| C/汇编引导、VBE、GDT、保护模式、FAT16 | [rewrite/boot/](rewrite/boot/) |
| INIT 启动与文件加载 | [rewrite/init/](rewrite/init/) |
| 固定地址内核源码与区间清单 | [rewrite/kernel/](rewrite/kernel/)、[patches.json](rewrite/kernel/patches.json) |
| X 程序加载、重定位与任务启动 | [x_loader.c](rewrite/kernel/x_loader.c)、[kernel_x_loader.py](rewrite/tests/kernel_x_loader.py) |
| 五个可重新编译的应用 | [rewrite/apps/](rewrite/apps/) |
| 每个原始程序的恢复状态 | [INVENTORY.md](rewrite/recovery/INVENTORY.md) |
| 实现细节、验证范围与阶段记录 | [rewrite/README.md](rewrite/README.md) |

当前的 C 源码恢复构建从根目录运行：

```sh
# 依赖：GCC/binutils（支持 i386）、make、Python 3、QEMU；硬件交接测试另需 GDB。
mkdir -p rewrite/build
python3 -m pip install --target rewrite/build/test-deps -r rewrite/tests/requirements.txt
make -C rewrite build/OpenIceStarOS_c_apps.vhd
make -C rewrite test-boot-video test-boot-stage3
make -C rewrite test-boot-stage3-qemu
make -C rewrite verify
make -C rewrite run-c-apps
```

`make -C rewrite verify` 运行原版与恢复代码的差分测试、硬件测试、
应用测试和整盘字节校验，耗时较长。启动、鼠标、键盘、内存和任务检查：

```sh
python3 rewrite/tools/boot_smoke.py --input-check --memory-check --task-check
python3 rewrite/tools/boot_smoke.py --menu-choice 2 --input-check
python3 rewrite/tools/boot_smoke.py --vga cirrus --input-check
```

这些命令成功表示其记录的场景通过，无法证明所有未恢复组件的行为。
截图、内存转储和 JSON 证据位于 `rewrite/build/`。

根目录的 `make verify` 是较早的前两阶段逐字节汇编恢复，构建
`build/OpenIceStarOS_mbr_rebuilt.vhd`；OpenIceStarOS 的当前源码恢复与集成请使用
`make -C rewrite ...`。Git 仓库位于根目录，每个验证后的步骤分别提交。
