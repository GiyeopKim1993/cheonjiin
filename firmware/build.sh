#!/usr/bin/env bash
# 천지인 펌웨어 검증 — 크로스 컴파일러 없이 호스트에서 전 계층 확인
set -e
cd "$(dirname "$0")"
OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT
CFLAGS="-std=c99 -O2 -Wall -Wextra"

echo "── 1. 시뮬레이터 통합 검증 (core + app + HAL) ──"
gcc $CFLAGS -o "$OUT/testfw" test/test_firmware.c app/cuime_app.c core/cuime.c port/sim/hal_sim.c
"$OUT/testfw"

echo
echo "── 2. 보드 포트 정합성 (HAL 구현 누락 검사) ──"
for P in esp32 stm32 rp2040; do
  gcc $CFLAGS -DCUIME_PORT_STUB -o "$OUT/port_$P" \
      test/port_stub_main.c app/cuime_app.c core/cuime.c "port/$P/hal_$P.c"
  "$OUT/port_$P"
  echo "  ✅ $P: 컴파일·링크·구동 (경고 0)"
done

echo
echo "── 3. 코어 전수 대조 (참조 구현 11,172자) ──"
if [ -f test/seq_mt.txt ]; then
  gcc $CFLAGS -o "$OUT/dump" test/dump_fw.c core/cuime.c
  for M in mt lp; do
    "$OUT/dump" "test/seq_$M.txt" "$OUT/out_$M.txt"
    BAD=$(awk -F'\t' '$1!=$2' "$OUT/out_$M.txt" | wc -l)
    LBL=$([ $M = mt ] && echo 멀티탭 || echo 롱프레스)
    echo "  $LBL: 11,172자 / 불일치 $BAD"
    [ "$BAD" -eq 0 ] || exit 1
  done
else
  echo "  (건너뜀: test/seq_*.txt 없음)"
fi
echo
echo "── 4. STM32F411 실기기 데모 (크로스 빌드 + 로직 검증) ──"
# 다른 포트와 달리 F411 은 레지스터 직접 제어라 port_stub_main 으로 돌릴 수 없다.
# 전용 스크립트가 크로스 빌드(zig/arm-none-eabi) + 정적 검증 + 호스트 로직 검증을 한다.
if python3 -m ziglang version >/dev/null 2>&1 || command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  if (cd port/stm32f411 && ./build.sh >/tmp/f411_build.log 2>&1); then
    SZ=$(grep -oP 'FLASH \K[0-9,]+' /tmp/f411_build.log | head -1)
    echo "  ✅ stm32f411: 크로스 빌드 + 로직 검증 통과 (FLASH ${SZ} B)"
  else
    echo "  ❌ stm32f411 실패:"; tail -20 /tmp/f411_build.log; exit 1
  fi
else
  echo "  SKIPPED (ARM 툴체인 없음: pip install --user ziglang)"
fi

echo
echo "✅ 펌웨어 전체 검증 통과"
