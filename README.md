# OpenIceStarOS x86 反编译与源码恢复

原始磁盘为 `IceStarOS_x86_32M.vhd`。FAT16 文件已提取到本地的
`IceStarOS_x86_32M_extracted/`；提取结果和构建产物由 Git 忽略。
所有测试镜像均写入 `build/` 或 `rewrite/build/`，运行时使用 QEMU
快照。原始 VHD 保留作为反编译和差分验证的参考。

许可范围见 [LICENSE](LICENSE)：自主编写且有权许可的 C 重写代码、
反编译辅助脚本及其他原创代码采用
[Apache License 2.0](LICENSES/Apache-2.0.txt)；原 VHD、非原创的
反编译／反汇编还原代码等材料保留原权利，原产权人为大虾
（All rights reserved），其他第三方权利仍归各自权利人。
原产权人如不允许公开相关还原代码，可向
[leonmmcoset@outlook.com](mailto:leonmmcoset@outlook.com) 发送关闭仓库请求。

**完整操作系统尚未恢复。** 当前可从源码编译三个引导阶段，第三阶段
已移除对原始引导机器码的 `.incbin` 引用。INIT 和内核仍为部分恢复；
内核有 607 个已安装的源码替换区间。已恢复并编译五个应用：SNAKE、
COLOR、MOV、STARSCR、CLOCK。原盘共 39 个 `.X`/`.DX` 路径、37 个不同
程序，仍包含未恢复的驱动和应用。区间数量不能当作完成比例。

| 内容 | 源码与说明 |
| --- | --- |
| C/汇编引导、VBE、GDT、保护模式、FAT16 | [rewrite/boot/](rewrite/boot/) |
| INIT 启动与文件加载 | [rewrite/init/](rewrite/init/) |
| PNG／DEFLATE 解码、滤波和图像生命周期 | [PNG.md](rewrite/recovery/PNG.md)、[INFLATE.md](rewrite/recovery/INFLATE.md) |
| 固定地址内核源码与区间清单 | [rewrite/kernel/](rewrite/kernel/)、[patches.json](rewrite/kernel/patches.json) |
| X 程序加载、重定位与任务启动 | [x_loader.c](rewrite/kernel/x_loader.c)、[kernel_x_loader.py](rewrite/tests/kernel_x_loader.py) |
| 字体解析、CFF 绘图、轮廓边界与内存池 | [FONT_OUTLINE.md](rewrite/recovery/FONT_OUTLINE.md)、[FONT_HEAP.md](rewrite/recovery/FONT_HEAP.md) |
| TrueType 简单／复合字形解码与形状分派 | [FONT_SHAPE.md](rewrite/recovery/FONT_SHAPE.md) |
| 字体活动边、扫描线覆盖与边排序 | [FONT_SCAN.md](rewrite/recovery/FONT_SCAN.md) |
| 字体二次／三次曲线细分与两阶段轮廓展开 | [FONT_FLATTEN.md](rewrite/recovery/FONT_FLATTEN.md) |
| 字体边构建、字形／码点位图分配与渲染 | [FONT_RENDER.md](rewrite/recovery/FONT_RENDER.md) |
| 字体图集上下文、矩形排布与绘制坐标 | [FONT_ATLAS.md](rewrite/recovery/FONT_ATLAS.md) |
| 字符烘焙、图集像素渲染、多范围打包与缩放度量 | [FONT_BAKE.md](rewrite/recovery/FONT_BAKE.md) |
| 字体名称、Unicode、TrueType 文本绘制 | [FONT_TEXT.md](rewrite/recovery/FONT_TEXT.md) |
| 窗口、事件回调与图像服务 | [WINDOW_SERVICES.md](rewrite/recovery/WINDOW_SERVICES.md) |
| 窗口创建、关闭与图像控件生命周期 | [WINDOW_LIFECYCLE.md](rewrite/recovery/WINDOW_LIFECYCLE.md)、[IMAGE_CONTROLS.md](rewrite/recovery/IMAGE_CONTROLS.md) |
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

OpenIceStarOS 的品牌安装、兼容名称保留项和磁盘残留说明见
[`rewrite/branding/README.md`](rewrite/branding/README.md)。实际 QEMU 启动菜单、
桌面、新品牌 CMD 与 C CLOCK 截图保存在
[`rewrite/evidence/2026-10-28-branding/`](rewrite/evidence/2026-10-28-branding/)。
CMD/TASKBAR 的显示文字补丁不代表这些程序已恢复为 C。

根目录的 `make verify` 是较早的前两阶段逐字节汇编恢复，构建
`build/OpenIceStarOS_mbr_rebuilt.vhd`；OpenIceStarOS 的当前源码恢复与集成请使用
`make -C rewrite ...`。Git 仓库位于根目录，每个验证后的步骤分别提交。

2026-11-01 的 601 区间版本累计新增 51 个实际 C 入口，已通过原版／普通 C／固定
地址 C 差分，以及覆盖新增函数体的真实 QEMU 诊断。生产 COLOR、输入、内存、
任务切换、整盘安装和清单检查通过，证据与实际截图见
[窗口与图像服务验证](rewrite/recovery/WINDOW_SERVICES.md)。完整 OS 仍未完成。
601 区间版本的完整 `make -C rewrite verify` 已于 2026-11-01 以退出码 0 完成，
覆盖到最终应用整盘安装与清单检查，详见
[全量回归记录](rewrite/evidence/2026-11-01-kernel-601-regression/README.md)。
专项 QEMU 证据另行保存；原 Logo 保持不变。

后续又恢复图像控件创建、释放、更换文件和绘制四个 C 函数，达到 605 个区间。
专项差分与真实 FAT／PNG／BMP／JPEG／ICO 图像链、窗口邻近回归和生产启动通过，
详见[图像控件恢复](rewrite/recovery/IMAGE_CONTROLS.md)。

随后恢复窗口创建和关闭两个主函数，达到 607 区间。本次两批共新增 6 个函数，
2,024 个三版本差分场景通过；真实 QEMU 覆盖四种窗口样式、屏幕尺寸缓冲、
关闭回调、完成通知及 31 个邻近入口，生产 CLOCK、整盘和清单检查通过。
详见[窗口生命周期恢复](rewrite/recovery/WINDOW_LIFECYCLE.md)及其实际截图。
607 区间仅完成上述专项与邻近回归，最后一次全量回归仍为 601 区间。完整 OS 未完成。
