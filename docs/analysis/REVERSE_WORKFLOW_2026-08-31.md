# PC12M-2 逆向与重建流程

> 整理日期：2026-08-31。

1. **P0 备份**：ISP 读取 256 KiB，核对型号、向量、CRP、哈希和重读一致性。
2. **P1 反汇编**：从十二相原 BIN 建立函数表，导出 raw 与带引用注释的 functions 两套证据。
3. **P2 数据层**：解出 SRAM 初始镜像、literal 池、全局地址和字符串池。
4. **P3 源码迁移**：借助成熟项目的同构结构理解语义，但把地址、位宽、常量和控制流逐项替换为十二相证据。
5. **P4 人工复核**：逐模块核对十二相反汇编，保留真实差异。
6. **P5 执行级验证**：Unicorn 执行原 BIN 与新 ELF，比较返回值、SRAM 和 MMIO/GPIO 写迹。
7. **W8 实机验证**：备份闭环和 P5 全过后，按控制电、空载触发、通信标定、低压限流、带载分级推进。

当前权威反汇编目录为 `evidence/reverse/disassembly/functions/`，完整 A/B 入口为
`tools/verification/verify_firmware_equivalence_12.py`。
