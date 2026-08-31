# PC12M-2 菜单状态机

> 整理日期：2026-08-31。

- 十二相 `state_machine` 入口为 `0x4464`。
- 当前 C 实现位于 `firmware/src/07_state_machine.c`。
- 带 literal/ref 注释的权威反汇编为
  `evidence/reverse/disassembly/functions/00004464_FUN_00004464.txt`。
- `evidence/reverse/notes/` 保存各菜单 case 的人工拆解过程。
- 菜单、光标、按键和显示刷新必须按十二相 SRAM 槽与调用地址验证。
- 完整状态机矩阵属于 P5 未完成部分；当前阻塞首先发生在输入消抖计数器错位。

原始结构与人工还原发生冲突时，以十二相原 BIN 执行结果为最终标准。
