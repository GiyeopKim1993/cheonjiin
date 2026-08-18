#!/usr/bin/env bash
# 외부 SDK 자동 취득/제거 — 워크스페이스에 남기지 않는다
#
# 정책: SDK 는 빌드할 때만 받아 쓰고 끝나면 지운다. 저장소에 벤더 코드를
#       커밋하지 않는다(라이선스·용량·리뷰 부담).
#
# 받는 위치: /tmp/cuime-sdk/   ← 워크스페이스 밖. 스냅샷에 안 잡힘
#
# 사용법:
#   ./fetch_sdk.sh tinyusb      # 받기 (이미 있으면 재사용)
#   ./fetch_sdk.sh --clean      # 지우기
#   ./fetch_sdk.sh --path       # 경로만 출력 (스크립트에서 사용)
set -euo pipefail

SDK_ROOT="${CUIME_SDK_ROOT:-/tmp/cuime-sdk}"
TINYUSB_REPO="https://github.com/hathach/tinyusb.git"
# 재현성을 위해 태그 고정. 올릴 때는 이 값만 바꾼다.
TINYUSB_TAG="0.16.0"

usage() { sed -n '2,20p' "$0"; exit 1; }

clean_all() {
    if [ -d "$SDK_ROOT" ]; then
        rm -rf "$SDK_ROOT"
        echo "🧹 SDK 제거: $SDK_ROOT"
    else
        echo "🧹 제거할 SDK 없음"
    fi
    # 워크스페이스에 실수로 들어온 흔적도 확인
    local ws
    ws="$(cd "$(dirname "$0")/../.." && pwd)"
    local stray
    stray=$(find "$ws" -maxdepth 3 -type d \( -name tinyusb -o -name 'tinyusb-*' \) 2>/dev/null || true)
    if [ -n "$stray" ]; then
        echo "⚠️  워크스페이스에 SDK 흔적이 있다:"
        echo "$stray"
        echo "   rm -rf 로 지워라 (저장소에 커밋되면 안 된다)"
        return 1
    fi
    return 0
}

fetch_tinyusb() {
    local dst="$SDK_ROOT/tinyusb"
    if [ -f "$dst/src/tusb.h" ]; then
        echo "♻️  TinyUSB 재사용: $dst"
        return 0
    fi
    mkdir -p "$SDK_ROOT"
    echo "⬇️  TinyUSB $TINYUSB_TAG 취득 중…"
    # 전체 이력 불필요 — 얕은 클론
    if ! git clone --depth 1 --branch "$TINYUSB_TAG" -q "$TINYUSB_REPO" "$dst" 2>/dev/null; then
        echo "❌ TinyUSB 취득 실패 (네트워크?)"
        return 1
    fi
    # 우리가 쓰지 않는 대용량 디렉터리 제거 — /tmp 는 993M 뿐이다
    rm -rf "$dst/.git" "$dst/hw/bsp" "$dst/examples" "$dst/docs" "$dst/test" 2>/dev/null || true
    echo "✅ TinyUSB: $dst ($(du -sh "$dst" 2>/dev/null | cut -f1))"
}

[ $# -ge 1 ] || usage

case "$1" in
    --clean) clean_all ;;
    --path)  echo "$SDK_ROOT" ;;
    tinyusb) fetch_tinyusb ;;
    *)       usage ;;
esac
