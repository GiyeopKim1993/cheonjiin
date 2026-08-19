#!/usr/bin/env bash
# Windows TSF DLL 실빌드 (Linux 에서 크로스 컴파일)
#
# MSVC 없이 zig(clang+MinGW 헤더)로 실제 PE DLL 을 만든다.
# Windows 에서는 다음과 동등:
#   cl /std:c++17 /LD /EHsc /utf-8 tsf_textservice.cpp /link ole32.lib ...
#
# zig 설치: python3 -m pip install --user ziglang
set -euo pipefail
cd "$(dirname "$0")"
OUT="${1:-cuime.dll}"

if ! python3 -m ziglang version >/dev/null 2>&1; then
  echo "⏭  zig 없음 — 건너뜀 (설치: python3 -m pip install --user ziglang)"
  exit 0
fi

python3 -m ziglang c++ -target x86_64-windows-gnu -std=c++17 -O2 -shared \
  -Wno-nullability-completeness \
  -o "$OUT" tsf_textservice.cpp \
  -lole32 -loleaut32 -ladvapi32 -luser32

python3 ../../tools/check_pe_exports.py "$OUT"
echo "✅ DLL 빌드 완료: $OUT"
echo "   설치(Windows, 관리자): regsvr32 $OUT"
