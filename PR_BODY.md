# 천지인 IME — 코어 5개 언어 + 5개 플랫폼 + 물리 키보드 펌웨어

4×4(16키) 천지인 한글 입력기. 플랫폼 독립 코어를 기준 구현으로 두고,
공용 셸 위에 플랫폼 바인딩을 얹는 구조입니다.

## 무엇이 들어있나

| 구분 | 내용 |
|---|---|
| **코어** | `cheonjiin.js` / `.hpp` / `.java` / `.py` / `.swift` — 5개 언어 |
| **셸** | Web · Android · IBus(Linux) · IMK(macOS) · TSF(Windows) |
| **iOS** | Keyboard Extension (marked text 정식 지원) |
| **펌웨어** | 4계층 구조, ESP32 / STM32 / RP2040 + POSIX 시뮬레이터 |
| **사전** | 한자 5,208자 / 단어 29,361개 |
| **CI** | 9잡 — APK·DLL 실아티팩트 + ARM 크로스 컴파일 |
| **문서** | 기획 → 스펙 → 디자인 → 기능명세 (`docs/01`~`07`) |

## 검증

핵심 불변식은 **"5개 언어 구현이 한 글자도 다르게 동작하지 않는다"** 입니다.
11,172자(현대 한글 전체) 키 시퀀스를 전수 덤프해 MD5로 대조합니다.

```
✅ python == js     372100db39c6a5e64a6473f451823935
✅ python == java   372100db39c6a5e64a6473f451823935
✅ python == cpp    372100db39c6a5e64a6473f451823935
✅ python == swift  372100db39c6a5e64a6473f451823935
```

| 항목 | 결과 |
|---|---|
| 코어 4/4 전수 | 0 실패 |
| 셸 (Web 28 / Android / IBus / IMK 20 / TSF 25) | 전 항목 통과 |
| iOS 브리지 (marked text ↔ 폴백 동치, 11,172자) | 불일치 0 |
| 펌웨어 통합 21항목 + 전수 22,344회 | 불일치 0 |
| 보드 포트 3종 정합성 | 컴파일·링크·구동, 경고 0 |
| 사전 정밀도 (Unihan 음가 전수) | 오류 0건 |

재현: `./run-all-tests.sh` (3~6분)

## 설계상 중요한 결정

**펌웨어 테이블은 손으로 옮기지 않고 생성합니다.** `tools/gen_firmware_tables.py`
가 참조 구현에서 `cuime_tables.h` 를 만들고, CI가 `git diff --exit-code` 로
커밋본과의 일치를 강제합니다. 실제로 백스페이스 역추적 테이블을 재구현했다가
`나`⌫ 가 `니` 대신 `내` 로 가는 버그를 겪었습니다.

**HID 키보드는 유니코드를 보낼 수 없습니다.** 조합된 음절을 두벌식 자판
위치로 분해해 전송하고 호스트 IME 가 합칩니다(`가` → `r` `k`). 드라이버
설치 없이 일반 블루투스/USB 키보드로 인식됩니다.

**iOS 제약은 전제가 틀렸습니다.** 기존 스펙은 "`UITextDocumentProxy` 에
marked text API 가 없다"는 이유로 iOS 를 v1.2 로 미뤘으나,
[`setMarkedText(_:selectedRange:)`](https://developer.apple.com/documentation/uikit/uitextdocumentproxy)
는 **iOS 13.0+ Required 멤버**입니다. 근거가 됐던 자료가 2014년 iOS 8 시절
답변이라 낡았습니다. 정식 preedit 로 구현하고 v1.0 에 편입했습니다.

## 라이선스 — 코드와 사전이 다릅니다

| 대상 | 라이선스 |
|---|---|
| 코드 전체 | **PolyForm Strict 1.0.0** (열람·비영리만, 수정·재배포·상업이용 금지) |
| 한자 사전 4개 파일 | **CC BY-SA 4.0** |

단어 사전은 한국어 위키낱말사전 파생이라 **ShareAlike** 와
**No additional restrictions** 조항 때문에 비영리 조건을 붙일 수 없습니다.
법적 강제 사항이라 분리했습니다. 자세한 내용은 `LICENSE-DICT.md`.

사전이 필요 없으면 4개 파일을 빼고 코드만 쓸 수 있습니다(코어·펌웨어는
사전에 의존하지 않음 — 검증 완료).

## 이 PR 의 변경

- 빈 `readme.md` → 내용이 있는 `README.md` 로 대체
  (git 은 대소문자를 구분해 두 파일이 공존하면 혼란스러움)
- 나머지 132개 파일 신규 추가

## 머지 후 확인 필요

CI 9잡이 처음 실행됩니다. 로컬에서 검증 불가능했던 항목이 여기서 실증됩니다.

- `android` — APK 아티팩트
- `windows-tsf` — MSVC DLL + COM 내보내기 검사
- `ios` — macOS 러너에서 실제 UIKit arm64 컴파일
- `firmware` — 실제 `arm-none-eabi-gcc` 크로스 컴파일
- `e2e` — 실제 Chrome 28건

Actions 권한이 `Read and write` 여야 아티팩트 업로드가 됩니다
(Settings → Actions → General → Workflow permissions).
