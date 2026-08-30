# W8 新固件烧写后记录（2026-08-26）

> 前置：自编译固件存在 CRP 地址布局冲突（`0x2FC` 落在 `wd_feed()` 指令中），一度被
> 拦在烧写前，详见 `W8_ISP_FLASH_2026-08-26.md` §8.4。本文记录 **CRP 布局修复完成、并经
> J-Link SWD 将修复后的自编译固件烧写入板**的完整过程、双重复核结果与当前板上状态。
> 总入口见 `W8_ONBOARDING_2026-08-22.md`。

## 0. 一句话结论

对 `firmware/` 做三处 CRP 布局修复（`startup.s` 新增 `.crp` 段、`lpc1765.ld` 固定
`.crp` 到 `0x2FC..0x2FF`、`build.sh` 二进制输出 `--gap-fill 0xFF`），重建后四道烧写硬门槛
全过，经 J-Link SWD 整片擦除 + 写入 + 全镜像校验，**新固件已成功落盘**。板上独立回读确认
`0x2FC = 0xFFFFFFFF`（无保护）、向量表校验字正确。烧写后核心处于 halt 状态，需复位才运行。

## 1. 固件与构建（CRP 修复）

| 项 | 值 |
|---|---|
| 目标固件 | `firmware/firmware.bin` |
| 大小 | 65500 B（0xFFDC） |
| SHA-256 | `A61324DFB4841615F937A4EE798AA31B3DDCF519899C2ED037A2C0773DDA820E` |
| 构建 | Arm GNU Toolchain 14.2.Rel1，`firmware/build.sh`（text 61940 / data 3000 / bss 2188） |

修复内容：

1. **`firmware/startup.s`**：新增 `.crp` 输入段，发射 `LONG 0xFFFFFFFF`（LPC17xx 代码读保护字）。
2. **`firmware/lpc1765.ld`**：在 `.isr_vector` 与 `.text` 之间插入
   ```ld
   .crp 0x2FC : { KEEP(*(.crp)) } > FLASH
   ```
   使 `.text` 从 `0x300` 开始，普通代码绕开 `0x2FC..0x2FF`。
3. **`firmware/build.sh`**：`objcopy -O binary` 加 `--gap-fill 0xFF`，让向量表与 `.text`
   之间的间隙（`0xCC..0x2FB`）以擦除态填充。

修复后关键符号地址：

```text
_crp_word       = 0x000002FC   （CRP 控制字，0xFFFFFFFF）
Reset_Handler   = 0x00000300   （原先 0xCC，现移过 CRP 字）
wd_feed         = 0x00000528   （原先 0x2F4，不再穿越 0x2FC）
_etext          = 0x0000D2E8
```

## 2. 四道烧写硬门槛验证（§8.4 后续要求）

| # | 门槛 | 结果 |
|---|---|---|
| 1 | BIN/HEX `0x2FC` 严格等于 `0xFFFFFFFF` | ✓（BIN `xxd -s 0x2FC`；HEX 记录 `:0402FC00FFFFFFFF02`） |
| 2 | 向量表校验和及段地址 | ✓ 前 8 字求和 = `0x00000000`；复位向量 `0x301`；estack `0x100029C8` |
| 3 | 全量离线测试 | ✓ `python test/run_tests.py` 模块 11/11 |
| 4 | 原 BIN / 新 ELF A/B 执行级验证 | ✓ `verify_firmware_equivalence.py` 全部 PASS（VECTOR/UART_RX/PIN_CONFIG/GPIO2_INIT/AUTH_CHALLENGE/TIMER1_ISR/EINT1-3_ISR/TIMER2_ISR；矩阵：TIMER1 126、CRC 5、MODBUS 读65/写320、CLOSED_LOOP 6、OUTPUT_STAGE 144、STATE_MACHINE 115） |

## 3. SWD 烧写会话（2026-08-26）

前置：市电 / 门极 / 功率负载已断开，仅控制电供电（W8 阶段 0）。

J-Link Commander（V9.70，`D:\software\SEGGER\JLink_V970\JLink.exe`），
`-device LPC1765 -if SWD -speed 4000`，脚本 `backup/jlink_flash_new_firmware.jlink`：

```text
erase                      整片擦除完成（2.589s，Erase: 2.589s）
loadbin firmware.bin,0x0   Bank0 65536 B 下载（Program&Verify 0.379s，168 KB/s）
verifybin firmware.bin,0x0 Verify successful（读回 65500 B 全匹配）
```

过程日志：`backup/swd_flash_new_firmware.log`（本机忽略目录）。

## 4. MEMMAP 向量重映射排查（重要教训）

烧写后 `mem32 0x00000000` 读到 `10001FFC / 1FFF0081 / 全 0`，一度疑似"扇区 0 未擦净"。
实为 **LPC17xx 向量重映射**：J-Link 的 reset & halt 把核心停在 Boot ROM，此时 `MEMMAP=0`
（`0x400FC040`），**Boot ROM 向量表被映射到地址 `0x0`**，`mem32` 读到的是 Boot ROM 而非 Flash。

- **读真实 Flash 前先写 `w4 0x400FC040, 1`（MEMMAP=1，用户 Flash 向量）。**
- 此前的 `savebin` 全量 dump 中 99.62% 为 `0xFF`，说明 ISP 擦除本身是成功的；"残留内容"
  只是映射假象。
- `verifybin` 走 J-Link flash loader 直读真实 Flash，不受此影响——以 `verifybin` 为准。

## 5. 板上确认结果（MEMMAP=1 后独立回读）

```text
0x00000000 = 100029C8 | 00000301 | 00000349 | 0000034B | 0000034D | 0000034F | 00000351 | EFFFC2B6
0x000002FC = FFFFFFFF
0x00000300 = Reset_Handler 机器码（48 28 49 28 …）
```

| 检查项 | 板上读回 | 预期 | 结果 |
|---|---|---|---|
| 栈顶 `0x0` | `0x100029C8` | 新固件 estack | ✓ |
| 复位向量 `0x4` | `0x301`→`0x300` | `Reset_Handler` | ✓ |
| 向量表校验字 `0x1C` | `0xEFFFC2B6` | 新固件 `_vector_checksum` | ✓ |
| **CRP 字 `0x2FC`** | **`0xFFFFFFFF`** | 无保护 | ✓ |
| 全镜像 verifybin | Verify successful | 与 `firmware.bin` 一致 | ✓ |

## 6. 当前状态与后续调试指引

- **核心处于 halt 状态**（J-Link 烧写后未 `r`/`g`）。需复位（P12-7 或板载复位键）或
  断电重上电，固件才会运行。
- **运行后 P1.30(SWDIO)→AD0.4、P1.29(SWCLK)→RS485 DE**，SWD 将失联——这是已确证的设计
  行为（`W8_JLINK_DEBUG_2026-08-24.md` §3.1）。后续调试需 **connect-under-reset**（另接
  nRESET）或走 ISP 恢复通道。
- 后续 W8 分级验证按 `W8_HARDWARE_TEST_2026-08-22.md` 执行，不分级不得带载。

## 7. 记录回填模板

```text
[新固件烧写]
固件文件 / SHA-256：
J-Link 版本 / 接口 / 速率：
erase 耗时 / loadbin 耗时 / verifybin 结果：
MEMMAP=1 回读 0x2FC：
复位后首屏现象：
判定：PASS / FAIL
操作者与日期：
```

## 8. 本次执行记录

- 操作者确认：市电/门极/功率负载已断开，仅控制电（阶段 0）。
- J-Link V9.70 SWD @4000 kHz，连接 LPC1765（Cortex-M3 r2p0）成功。
- 烧写前整片 dump 备份：`backup/current_flash.bin`（262144 B，SHA-256
  `24E9CE2C84ADC3DAE631FD3D62EAA8389FA7C2F80CFA2D49498FEF02790E2513`），99.62% 为 `0xFF`。
- 会话序列：erase（2.589s）→ loadbin（0.498s）→ verifybin（Verify successful）→
  `w4 0x400FC040,1` 后独立回读确认。
- 日志：`backup/swd_flash_new_firmware.log`。
