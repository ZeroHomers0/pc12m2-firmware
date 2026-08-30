#!/usr/bin/env bash
# =============================================================================
# firmware/build.sh — 目标B 骨架构建（等价于 make，无需 make 在 PATH）
# 用法：bash build.sh
# 产出：firmware.elf / firmware.hex / firmware.bin / firmware.map
# =============================================================================
set -e
cd "$(dirname "$0")"

# 工具链自动定位：约定版本(14.2 rel1)优先 → 常见安装目录最新版本 → PATH
TC=""
TC_OLD="/c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin"
if [ -d "$TC_OLD" ]; then
  TC="$TC_OLD"
else
  for base in "/c/Program Files (x86)/Arm GNU Toolchain arm-none-eabi" \
              "/c/Program Files/Arm GNU Toolchain arm-none-eabi"; do
    if [ -d "$base" ]; then
      TC=$(find "$base" -maxdepth 2 -name bin -type d 2>/dev/null | sort -V | tail -1)
      [ -n "$TC" ] && break
    fi
  done
fi
if [ -z "$TC" ] && command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  TC="$(dirname "$(command -v arm-none-eabi-gcc)")"
fi
if [ -z "$TC" ]; then
  echo "错误: 找不到 Arm GNU Toolchain（arm-none-eabi-gcc）。" >&2
  echo "       请按 操作文档.md §1 手动安装 Arm GNU Toolchain。" >&2
  exit 1
fi
CC="$TC/arm-none-eabi-gcc"
OBJCOPY="$TC/arm-none-eabi-objcopy"

CPU="-mcpu=cortex-m3 -mthumb -mfloat-abi=soft"
CFLAGS="$CPU -Os -ffreestanding -fno-builtin -Wall -I. -Iinc"
CFLAGS="$CFLAGS -Wno-unused-but-set-variable -Wno-unused-variable -Wno-pointer-sign"
CFLAGS="$CFLAGS -Wno-parentheses"
# -Wno-parentheses：反编译 Ghidra 风格 `a + b & 0xff`（& 优先级已低于 +，语义即
#   (a+b)&0xff）遍布各模块，括号仅为样式建议，不隐含真实错误 → 全局抑制。

rm -f *.o src/*.o firmware.elf firmware.hex firmware.bin firmware.map

echo "== 编译 =="
"$CC" $CFLAGS -c -o startup.o startup.s
"$CC" $CFLAGS -c -o data_image.o data_image.s
"$CC" $CFLAGS -c -o globals.o globals.c
"$CC" $CFLAGS -c -o stub.o stub.c
for f in src/*.c; do
  echo "  $f"
  "$CC" $CFLAGS -c -o "${f%.c}.o" "$f"
done

echo "== 链接 =="
"$CC" $CPU -T lpc1765.ld -nostdlib -o firmware.elf *.o src/*.o \
    -lgcc -Wl,--gc-sections -Wl,-Map,firmware.map

echo "== 产物 =="
"$OBJCOPY" -O ihex firmware.elf firmware.hex
# --gap-fill 0xFF：CRP 占位引入的 0xCC..0x2FB 间隙（以及任何未用区）以 0xFF
# 填充，与 Flash 擦除态一致，避免 J-Link 把无关的 0x00 写进该区域。
"$OBJCOPY" --gap-fill 0xFF -O binary firmware.elf firmware.bin
"$TC/arm-none-eabi-size" firmware.elf
echo "OK: firmware.elf / firmware.hex / firmware.bin"
