#!/usr/bin/env bash
# iOS 검증 — macOS/Xcode 없이 Linux 에서 실행 가능한 전부
#
#  1. 코어 전수 동일성 : Swift == Python/JS/Java/C++ (11,172자 MD5)
#  2. 브리지 검증      : marked text 경로 == iOS12 폴백 경로 (전수)
#  3. UIKit 타입체크   : 스텁으로 KeyboardViewController/KeypadView 검증
#  4. Info.plist 검증  : 확장 포인트·주 클래스
#
# 실기기/시뮬레이터 구동은 macOS + Xcode 가 필요하다.
set -euo pipefail
cd "$(dirname "$0")"

SWIFTC="${SWIFTC:-$(command -v swiftc || echo /home/user/swift/usr/bin/swiftc)}"
if [ ! -x "$SWIFTC" ]; then
  echo "⏭  Swift 툴체인 없음 — 건너뜀 ($SWIFTC)"
  exit 0
fi
# Debian 13 에는 gold 링커가 없다. lld 로 대체.
LD_FLAG=""
if "$SWIFTC" --version 2>/dev/null | grep -q "5\.\|6\."; then LD_FLAG="-use-ld=lld"; fi
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT

echo "── 1. 코어 전수 동일성 (11,172자) ──"
"$SWIFTC" -O $LD_FLAG -o "$TMP/xldump" CheonjiinCore.swift test/xldump/main.swift
"$TMP/xldump" "$TMP/swift.txt" 2>/dev/null
(cd ../../tools && python3 -c "
from engine import CHO,JUNG,JONG,compose
from encoder import encode
for c in CHO:
  for v in JUNG:
    for j in JONG:
      ch=compose(c,v,j.strip())
      print(ch+'\t'+' '.join('KRIGHT' if k=='TIMEOUT' else k for k in encode(ch)))" > "$TMP/py.txt")
HP=$(md5sum "$TMP/py.txt"    | cut -d' ' -f1)
HS=$(md5sum "$TMP/swift.txt" | cut -d' ' -f1)
if [ "$HP" = "$HS" ]; then
  echo "✅ Swift == Python  ($HS)"
else
  echo "❌ 불일치: python=$HP swift=$HS"; diff "$TMP/py.txt" "$TMP/swift.txt" | head -5; exit 1
fi

echo
echo "── 2. CompositionBridge (marked text / 폴백 동치) ──"
"$SWIFTC" -O $LD_FLAG -o "$TMP/bridge" \
  CheonjiinCore.swift CompositionBridge.swift test/bridge/main.swift
"$TMP/bridge"

echo
echo "── 3. UIKit 타입체크 (스텁) ──"
# UIKit 은 Linux 에 없다. import 를 Foundation 으로 바꾸고 스텁을 주입한다.
sed 's/^import UIKit$/import Foundation/' KeyboardViewController.swift > "$TMP/kvc.swift"
sed 's/^import UIKit$/import Foundation/' KeypadView.swift            > "$TMP/kp.swift"
"$SWIFTC" -typecheck test/uikit-stub/UIKitStub.swift \
  "$TMP/kvc.swift" "$TMP/kp.swift" CheonjiinCore.swift CompositionBridge.swift
echo "✅ KeyboardViewController + KeypadView 타입체크 통과"

echo
echo "── 4. Info.plist ──"
python3 - <<'PYEOF'
import plistlib
with open('Info.plist','rb') as f: d = plistlib.load(f)
ext = d['NSExtension']
assert ext['NSExtensionPointIdentifier'] == 'com.apple.keyboard-service', '확장 포인트 오류'
assert 'KeyboardViewController' in ext['NSExtensionPrincipalClass'], '주 클래스 오류'
a = ext['NSExtensionAttributes']
assert a['PrimaryLanguage'] == 'ko-KR', '언어 오류'
assert a['RequestsOpenAccess'] is False, 'Open Access 는 불필요해야 한다'
print('✅ Info.plist 유효 (keyboard-service / ko-KR / OpenAccess=false)')
PYEOF

echo
echo "✅ iOS 전 항목 통과"
echo "   (실기기·시뮬레이터 구동은 macOS + Xcode 필요)"
