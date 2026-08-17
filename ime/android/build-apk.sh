#!/usr/bin/env bash
# APK 실빌드 — Gradle 없이 Android SDK 툴만으로 직접 빌드
#
# Gradle 은 네트워크·데몬·메모리를 크게 요구한다. aapt2/javac/d8/apksigner 를
# 직접 호출하면 동일한 APK 를 훨씬 가볍게 만들 수 있고, 실패 지점이 명확하다.
# CI 에서도 이 스크립트를 그대로 쓴다.
set -euo pipefail
cd "$(dirname "$0")"

: "${ANDROID_HOME:=/home/user/android-sdk}"
: "${JAVA_HOME:=/home/user/jdk17}"
# 설치된 build-tools 중 최신 버전을 자동 선택 (CI/로컬 모두 동작)
BT="${BT:-$(ls -d "$ANDROID_HOME"/build-tools/* 2>/dev/null | sort -V | tail -1)}"
[ -n "$BT" ] && [ -d "$BT" ] || { echo "❌ build-tools 없음: $ANDROID_HOME"; exit 1; }
JAR="${JAR:-$(ls -d "$ANDROID_HOME"/platforms/android-* 2>/dev/null | sort -V | tail -1)/android.jar}"
[ -f "$JAR" ] || { echo "❌ android.jar 없음"; exit 1; }
# apksigner/d8 는 PATH 의 java 를 쓴다. JDK17 을 앞에 둬야 키스토어
# 알고리즘(HmacPBESHA256)을 읽을 수 있다.
export PATH="$JAVA_HOME/bin:$PATH"
JAVAC="$JAVA_HOME/bin/javac"

OUT=build-apk
rm -rf "$OUT"; mkdir -p "$OUT/gen" "$OUT/classes" "$OUT/dex"

echo "── 1. 리소스 컴파일 (aapt2) ──"
"$BT/aapt2" compile --dir src/main/res -o "$OUT/res.zip"

echo "── 2. 리소스 링크 + R.java 생성 ──"
"$BT/aapt2" link \
  -I "$JAR" \
  --manifest src/main/AndroidManifest.xml \
  --java "$OUT/gen" \
  --min-sdk-version 21 --target-sdk-version 34 \
  -o "$OUT/base.apk" \
  "$OUT/res.zip"

echo "── 3. 자바 컴파일 ──"
# -bootclasspath 는 target 8 이하에서만 허용된다. Android 는 8 이 표준이며,
# 이렇게 해야 android.jar 만 보이고 JDK 클래스가 섞이지 않는다.
"$JAVAC" -encoding UTF-8 -nowarn -Xlint:-options -source 8 -target 8 \
  -bootclasspath "$JAR" -cp "$JAR" -d "$OUT/classes" \
  $(find src/main/java "$OUT/gen" -name '*.java') 2>&1 | tee "$OUT/javac.log" | grep -vE "^(Note|warning:)" || true
# 파이프는 종료코드를 가린다. 로그에서 error 를 직접 검사한다.
if grep -q "error:" "$OUT/javac.log"; then
  echo "❌ 자바 컴파일 오류:"; grep "error:" "$OUT/javac.log" | head -10; exit 1
fi
CLASS_N=$(find "$OUT/classes" -name '*.class' | wc -l)
[ "$CLASS_N" -gt 0 ] || { echo "❌ 자바 컴파일 실패 (.class 0개)"; exit 1; }
echo "   .class $CLASS_N 개"

echo "── 4. DEX 변환 (d8) ──"
mapfile -t CLASSES < <(find "$OUT/classes" -name '*.class')
"$BT/d8" --min-api 21 --lib "$JAR" --output "$OUT/dex" "${CLASSES[@]}"
[ -f "$OUT/dex/classes.dex" ] || { echo "❌ classes.dex 생성 실패"; exit 1; }
echo "   classes.dex $(du -h "$OUT/dex/classes.dex" | cut -f1)"

echo "── 5. APK 패키징 ──"
cp "$OUT/base.apk" "$OUT/unsigned.apk"
(cd "$OUT/dex" && zip -q "../unsigned.apk" classes.dex)

echo "── 6. 정렬 + 서명 ──"
if [ ! -f "$OUT/debug.keystore" ]; then
  "$JAVA_HOME/bin/keytool" -genkeypair -keystore "$OUT/debug.keystore" \
    -storepass android -keypass android -alias androiddebugkey \
    -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Android Debug,O=Android,C=US" 2>/dev/null
fi
"$BT/zipalign" -f 4 "$OUT/unsigned.apk" "$OUT/cuime-debug.apk"
"$BT/apksigner" sign --ks "$OUT/debug.keystore" --ks-pass pass:android \
  --key-pass pass:android --ks-key-alias androiddebugkey "$OUT/cuime-debug.apk"

echo
echo "── 7. 검증 ──"
"$BT/apksigner" verify --print-certs "$OUT/cuime-debug.apk" | head -2
echo "패키지: $("$BT/aapt2" dump packagename "$OUT/cuime-debug.apk")"
# IME 서비스가 실제로 등록됐는지 확인 (이게 없으면 키보드로 뜨지 않는다)
"$BT/aapt2" dump xmltree "$OUT/cuime-debug.apk" --file AndroidManifest.xml \
  | grep -q "android.view.InputMethod" \
  && echo "IME 서비스 등록: ✅" || { echo "❌ IME 서비스 미등록"; exit 1; }
echo "APK 크기: $(du -h "$OUT/cuime-debug.apk" | cut -f1)"
echo
echo "✅ APK 빌드 성공: $OUT/cuime-debug.apk"
