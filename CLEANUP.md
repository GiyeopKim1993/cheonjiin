# 정리 시 주의

`ime/native/` 에는 **소스와 빌드 산출물의 이름이 겹친다**:

| 소스 (지우면 안 됨) | 산출물 (지워도 됨) |
|---|---|
| `test.cpp` | `test_cpp` |
| `test_shell.cpp` | `test_shell` |
| `tsf_textservice.cpp` | `test_tsf`, `cuime.dll` |
| `dump.cpp` | `dump` |

**금지**: `rm ime/native/test_*` — `test_shell.cpp` 까지 지운다. (2회 발생)

**안전한 정리**:
```bash
# 확장자 없는 실행 파일만 제거
find ime/native -maxdepth 1 -type f -perm -u+x ! -name '*.*' -delete
rm -rf ime/android/build ime/android/build-stub ime/android/build-apk
rm -f ime/native/*.dll ime/native/*.o
```

복구가 필요하면 `./run-all-tests.sh` 가 어떤 파일이 없는지 알려준다.

---

# 워크스페이스 스냅샷 한계 (실제 소실 사고)

턴 종료 스냅샷에는 **약 128MB / 10,000파일 상한**이 있다. 대용량 툴체인을
`/home/user/` 아래 설치하면 상한을 넘겨 **소스 파일까지 누락**될 수 있다.

실제로 JDK 93MB + Swift 600MB 를 설치한 턴에서 다음이 소실됐다:

- `ime/ios/` 전체
- `ime/native/tsf_com.inc`
- `ime/android/build-apk.sh`, `ime/native/build-dll.sh`
- `tsf_textservice.cpp` 의 `#include "tsf_com.inc"` (구버전으로 되돌아감)
- 스크립트 실행 권한(`chmod +x`)

## 예방 — 확정 방침

**워크스페이스에는 소스만 둔다.** SDK·툴체인은 설치하지 않으며,
필요하면 그 턴 안에서 쓰고 반드시 제거한다.

- `/tmp` 는 993MB tmpfs 라 Swift(639MB) 같은 큰 툴체인이 들어가지 않는다.
- `/home/user` 에 설치하면 스냅샷 상한(~128MB)을 넘겨 **소스가 소실된다**.
- 결론: 무거운 검증은 **CI 에서** 한다. 로컬은 gcc/g++/javac/node/python3
  (시스템 기본 제공)만으로 돌아가는 범위를 검증한다.

현재 워크스페이스: **8.7MB / 354파일** — 상한 대비 충분히 여유.

`run-all-tests.sh` 는 툴체인이 없으면 실패가 아니라 **건너뛰고**,
마지막에 건너뛴 항목을 모아 출력한다. "통과"와 "미실행"을 혼동하지 말 것.

## 복구 확인 명령

```bash
./run-all-tests.sh        # 어떤 파일이 없는지 즉시 드러난다
git status                # 저장소라면 삭제된 파일 목록
```

# 저메모리 환경 주의

`ime/native/hanja.hpp` 는 1.1MB 단일 헤더다. 가용 메모리가 ~1.2GB 이면
`g++ -O2` 가 **cc1plus OOM 으로 죽는다**(실제 발생). 사전 검증은 정확성이
목적이므로 `run-all-tests.sh` 는 `-O0` 를 쓴다. 바꾸지 말 것.
