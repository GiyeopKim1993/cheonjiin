#!/usr/bin/env bash
# STM32F411 데모 펌웨어 빌드
#
# 툴체인: arm-none-eabi-gcc 가 있으면 그것을, 없으면 zig 를 쓴다.
# CubeMX·CMSIS·HAL 라이브러리 없이 이 디렉터리 파일만으로 빌드된다.
#
# 옵션(환경변수):
#   USE_HSE=1   HSE 25MHz 크리스털 사용 (Black Pill). USB 쓰려면 필수
#   USB_HID=1   USB HID 두벌식 모드 (별도 USB 스택 링크 필요)
#
# 산출물: build/cuime-f411.{elf,bin,hex}
set -euo pipefail
cd "$(dirname "$0")"
FW=../..                       # firmware/ 루트
OUT=build
rm -rf "$OUT"; mkdir -p "$OUT"

DEFS=""
[ "${USE_HSE:-0}" = "1" ] && DEFS="$DEFS -DCUIME_F411_USE_HSE"
[ "${USB_HID:-0}" = "1" ] && DEFS="$DEFS -DCUIME_F411_USB_HID"

CFLAGS="-mcpu=cortex-m4 -mthumb -Os -Wall -Wextra -Werror -std=c99
        -ffreestanding -fno-builtin -ffunction-sections -fdata-sections
        -Iinclude $DEFS"

SRCS="startup_stm32f411.c hal_stm32f411.c main_stm32f411.c freestanding_libc.c
      $FW/core/cuime.c $FW/app/cuime_app.c"

if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  echo "── 툴체인: arm-none-eabi-gcc ──"
  CC="arm-none-eabi-gcc"
  $CC $CFLAGS -nostdlib -T stm32f411.ld -Wl,--gc-sections -Wl,-Map="$OUT/cuime-f411.map" \
      -o "$OUT/cuime-f411.elf" $SRCS
  arm-none-eabi-objcopy -O binary "$OUT/cuime-f411.elf" "$OUT/cuime-f411.bin"
  arm-none-eabi-objcopy -O ihex   "$OUT/cuime-f411.elf" "$OUT/cuime-f411.hex"
  arm-none-eabi-size "$OUT/cuime-f411.elf"
else
  echo "── 툴체인: zig (arm-none-eabi-gcc 없음) ──"
  command -v python3 >/dev/null && python3 -m ziglang version >/dev/null 2>&1 \
    || { echo "❌ zig 없음: python3 -m pip install --user ziglang"; exit 1; }
  ZCC="python3 -m ziglang cc -target thumb-freestanding-eabi -mcpu=cortex_m4"
  OBJS=""
  for f in $SRCS; do
    o="$OUT/$(basename "$f" .c).o"
    # zig 는 -mcpu 표기가 다르므로 CFLAGS 에서 gcc 전용 플래그를 뺀다
    $ZCC -Os -Wall -Wextra -Werror -ffreestanding -fno-builtin \
         -ffunction-sections -fdata-sections -Iinclude $DEFS -c "$f" -o "$o"
    OBJS="$OBJS $o"
  done
  $ZCC -nostdlib -Wl,-T,stm32f411.ld -Wl,--gc-sections \
       -o "$OUT/cuime-f411.elf" $OBJS
  python3 "$FW/tools/elf2bin.py" "$OUT/cuime-f411.elf" "$OUT/cuime-f411"
fi

echo
echo "── 정적 검증 (부팅 가능성) ──"
python3 "$FW/tools/verify_f411.py" "$OUT/cuime-f411.elf"

echo
echo "── 로직 검증 (호스트 실행) ──"
# 가짜 레지스터로 F411 HAL 을 x86 에서 구동해, 매트릭스 매핑·디바운스·
# UTF-8 인코딩과 "키 입력 -> 한글 조합 -> UART 출력" 전 경로를 실제로 돌린다.
# 실기기가 없어도 회귀를 잡을 수 있다.
if command -v gcc >/dev/null 2>&1; then
  gcc -std=c99 -O0 -Wall -Wextra -Werror -DCUIME_F411_HOSTTEST $DEFS \
      -o "$OUT/test_f411" \
      "$FW/test/test_f411.c" "$FW/test/f411_fake_regs.c" \
      "$FW/core/cuime.c" "$FW/app/cuime_app.c"
  "$OUT/test_f411" || { echo "❌ 로직 검증 실패"; exit 1; }
else
  echo "   SKIPPED (gcc 없음)"
fi

echo
echo "✅ 빌드 완료"
ls -la "$OUT"/cuime-f411.{elf,bin,hex} 2>/dev/null | awk '{printf "   %-28s %8s B\n", $9, $5}'
echo
echo "플래싱:"
echo "  st-flash write $OUT/cuime-f411.bin 0x08000000"
echo "  # 또는 dfu-util -a 0 -s 0x08000000:leave -D $OUT/cuime-f411.bin"
echo "  # 또는 openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \\"
echo "  #     -c \"program $OUT/cuime-f411.elf verify reset exit\""
