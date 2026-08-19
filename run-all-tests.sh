#!/bin/bash
# 천지인 IME 전체 검증 — 4개 언어 코어 + 3개 플랫폼 셸 + 교차 동일성
set -e
cd "$(dirname "$0")"
SKIPPED=""   # 툴체인 부재로 건너뛴 항목
echo "════════ 1. 코어 (4개 언어) ════════"
echo "── Python"; (cd tools && python3 verify_final.py | tail -3)
echo "── JavaScript"; (cd ime/core && node test.js | tail -3)
echo "── Java"; (cd ime/android && javac -encoding UTF-8 -d build src/main/java/com/cuime/core/CheonjiinCore.java test/CoreTest.java && java -Dfile.encoding=UTF-8 -cp build CoreTest | tail -3)
echo "── C++"; (cd ime/native && g++ -std=c++17 -O2 -o test_cpp test.cpp && ./test_cpp | tail -3)

echo; echo "════════ 2. 셸 (3개 플랫폼) ════════"
echo "── Web/JS"; (cd ime/core && node test-shell.js | tail -1)
echo "── Android/Java"; (cd ime/android && javac -encoding UTF-8 -nowarn -d build $(find stub -name '*.java') src/main/java/com/cuime/core/CheonjiinCore.java src/main/java/com/cuime/HanjaDict.java src/main/java/com/cuime/KeypadView.java src/main/java/com/cuime/CheonjiinIME.java test/ShellLogicTest.java && java -Dfile.encoding=UTF-8 -cp build ShellLogicTest | tail -1)
echo "── Linux/C++ (IBus)"; (cd ime/native && g++ -std=c++17 -O2 -I. -o test_shell test_shell.cpp && ./test_shell | tail -1)
echo "── macOS/C++ (IMK)"; (cd ime/macos && g++ -std=c++17 -O2 -I../native -DCUIME_IMK_TEST -DCUIME_IMK_NO_OBJC -x c++ -o /tmp/test_imk CheonjiinBridge.mm && /tmp/test_imk | tail -1)
echo "── Windows/C++ (TSF)"; (cd ime/native && g++ -std=c++17 -O2 -DCUIME_TSF_STUB -I. -o test_tsf tsf_textservice.cpp && ./test_tsf | tail -1)

echo; echo "════════ 2b. Android UI (SDK 스텁 타입체크 + 터치판정) ════════"
(cd ime/android && rm -rf build-stub && mkdir -p build-stub \
  && javac -encoding UTF-8 -nowarn -d build-stub $(find stub -name '*.java') \
       src/main/java/com/cuime/core/CheonjiinCore.java src/main/java/com/cuime/HanjaDict.java \
       src/main/java/com/cuime/KeypadView.java src/main/java/com/cuime/CheonjiinIME.java \
  && echo "✅ CheonjiinIME + KeypadView 컴파일 통과 (Android SDK 스텁)" \
  && javac -encoding UTF-8 -nowarn -cp build-stub -d build-stub test/KeypadViewTest.java \
  && java -Dfile.encoding=UTF-8 -cp build-stub KeypadViewTest | tail -1)

echo; echo "════════ 2c. 한자 사전 (4개 언어 대조) ════════"
python3 - <<'PYEOF'
import json
d=json.load(open('spec/hanja-dict.json'))
qs=list(d['words'].keys())+list(d['chars'].keys())
open('/tmp/queries.txt','w').write("\n".join(qs))
def lk(t):
    # words 값은 후보 배열(다중 후보). 옛 형식(문자열)도 받아들인다.
    if t in d['words']:
        v = d['words'][t]
        return v if isinstance(v, list) else [v]
    if len(t)==1 and t in d['chars']: return d['chars'][t]
    return []
with open('/tmp/hj_py.txt','w') as f:
    for q in qs: f.write(q+"\t"+" ".join(lk(q))+"\n")
print(f"   한자 {d['charCount']}자 / 음절 {d['syllableCount']} / 단어 {d['wordCount']} / 질의 {len(qs)}건")
PYEOF
(cd tools/xl && node dump_hanja.js)
(cd ime/android && javac -encoding UTF-8 -cp build -d build test/HanjaDumpTest.java && java -Dfile.encoding=UTF-8 -cp build HanjaDumpTest)
# 사전은 1.1MB 단일 파일이라 -O2 시 cc1plus 가 OOM 으로 죽는다(실제 발생).
# 정확성 검증이 목적이므로 -O0 로 충분하다.
(cd ime/native && g++ -std=c++17 -O0 -I. -o /tmp/hjd hanja_dump.cpp && /tmp/hjd)
HH=$(md5sum /tmp/hj_py.txt | cut -d' ' -f1)
for f in js java cpp; do
  [ -f /tmp/hj_$f.txt ] || continue
  h=$(md5sum /tmp/hj_$f.txt | cut -d' ' -f1)
  [ "$h" = "$HH" ] && echo "✅ 사전 python == $f" || { echo "❌ 사전 python != $f"; exit 1; }
done

echo; echo "════════ 2c-1. 사전 정밀도 (Unihan 음가 전수 대조) ════════"
if [ -f /tmp/Unihan_Readings.txt ]; then
  python3 tools/audit_dict.py /tmp/Unihan_Readings.txt | tail -4
else
  echo "⏭  Unihan 캐시 없음 — 건너뜀"
  echo "   실행: curl -sSL -o /tmp/Unihan.zip https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip \\"
  echo "         && (cd /tmp && unzip -o -q Unihan.zip Unihan_Readings.txt)"
  SKIPPED="$SKIPPED 사전정밀도(Unihan)"
fi

echo; echo "════════ 2c-3. 물리 키보드 브리지 (원시 키이벤트 -> IME) ════════"
(cd ime/device && node test-proto-sync.js | tail -2)
(cd ime/device && node test-bridge.js    | tail -1)
(cd ime/device && node test-transport.js | tail -1)

echo; echo "════════ 2c-2. iOS (Swift 코어 + 브리지 + UIKit 타입체크) ════════"
SWIFTC="${SWIFTC:-$(command -v swiftc || echo /home/user/swift/usr/bin/swiftc)}"
export SWIFTC          # 하위 스크립트로 전달 (없으면 ios/run-tests.sh 가 못 찾는다)
if [ -x "$SWIFTC" ]; then
  (cd ime/ios && ./run-tests.sh | grep -E "^(✅|❌)" | tail -8)
else
  echo "⏭  Swift 툴체인 없음 — 건너뜀 (iOS 코어·브리지·UIKit 타입체크)"
  SKIPPED="$SKIPPED iOS(Swift)"
fi

echo; echo "════════ 2d. 펌웨어 (core + app + HAL + 보드 포트) ════════"
(cd firmware && ./build.sh | grep -E "통합 검증|✅ (esp32|stm32|rp2040)|멀티탭:|롱프레스:")

echo; echo "════════ 3. 교차 언어 동일성 ════════"
mkdir -p /tmp/xl
(cd tools && python3 -c "
from engine import CHO,JUNG,JONG,compose
from encoder import encode
for c in CHO:
  for v in JUNG:
    for j in JONG:
      ch=compose(c,v,j.strip())
      print(ch+'\t'+' '.join('KRIGHT' if k=='TIMEOUT' else k for k in encode(ch)))" > /tmp/xl/py.txt)
(cd tools/xl && node dump_core.js > /tmp/xl/js.txt)
(cd ime/android && javac -encoding UTF-8 -cp build -d build test/Dump.java && java -Dfile.encoding=UTF-8 -cp build Dump)
(cd ime/native && g++ -std=c++17 -O2 -I. -o dump dump.cpp && ./dump)
# Swift(iOS) — 툴체인이 있을 때만
SWIFTC=${SWIFTC:-/home/user/swift/usr/bin/swiftc}
if [ -x "$SWIFTC" ]; then
  (cd ime/ios && "$SWIFTC" -O -use-ld=lld -o /tmp/xl/swdump CheonjiinCore.swift test/xldump/main.swift 2>/dev/null \
    && /tmp/xl/swdump /tmp/xl/swift.txt 2>/dev/null)
fi
H=$(md5sum /tmp/xl/py.txt | cut -d' ' -f1)
ALL_OK=1
for f in js java cpp swift; do
  [ -f /tmp/xl/$f.txt ] || continue
  h=$(md5sum /tmp/xl/$f.txt | cut -d' ' -f1)
  if [ "$h" = "$H" ]; then echo "✅ python == $f  ($h)"; else echo "❌ python != $f"; ALL_OK=0; fi
done
echo
NLANG=$(ls /tmp/xl/{js,java,cpp,swift}.txt 2>/dev/null | wc -l)
[ $ALL_OK -eq 1 ] && echo "✅✅ 전체 통과 — $((NLANG+1))개 언어가 11,172자 키 시퀀스까지 바이트 단위 동일" || exit 1

if [ -n "$SKIPPED" ]; then
  echo
  echo "⏭  건너뛴 항목:$SKIPPED"
  echo "   이 환경에 툴체인이 없어 미실행입니다. 통과가 아닙니다."
  echo "   전체 검증은 CI(.github/workflows/ci.yml) 에서 수행됩니다."
fi
