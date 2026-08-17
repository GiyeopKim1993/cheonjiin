# 사전 데이터 라이선스 — CC BY-SA 4.0

이 문서는 **한자 사전 데이터**에만 적용됩니다.
코드는 `LICENSE`(PolyForm Strict 1.0.0)를 따릅니다.

## 왜 별도 라이선스인가

단어 사전은 **한국어 위키낱말사전**에서 추출했습니다. 위키미디어 재단의
이용약관 §7에 따라 모든 텍스트 기여는 **CC BY-SA 4.0**으로 배포되며,
이 라이선스에는 재라이선싱을 막는 두 조항이 있습니다.

- **ShareAlike** — 2차 저작물은 반드시 동일 라이선스(CC BY-SA 4.0)로 배포
- **No additional restrictions** — 라이선스가 허용하는 행위를 막는
  추가 법적·기술적 제약을 붙일 수 없음

즉 이 데이터에 "비영리 전용"이나 "재배포 금지" 같은 조건을 붙이는 것은
**라이선스 위반**입니다. 따라서 코드와 분리해 CC BY-SA 4.0으로 둡니다.

> 출처: <https://foundation.wikimedia.org/wiki/Policy:Terms_of_Use> §7 Licensing of Content

## 적용 범위

| 대상 | 라이선스 | 근거 |
|---|---|---|
| `spec/hanja-dict.json` → `words` (29,361 단어) | **CC BY-SA 4.0** | 위키낱말사전 파생 |
| `spec/hanja-dict.json` → `chars` (5,208 매핑) | Unicode License + 공개표준 | 아래 참조 |
| `ime/core/hanja.js` | **CC BY-SA 4.0** | `words` 포함 |
| `ime/native/hanja.hpp` | **CC BY-SA 4.0** | `words` 포함 |
| `ime/android/.../HanjaDict.java` | **CC BY-SA 4.0** | `words` 포함 |
| `tools/fetch_words.py`, `tools/gen_hanja.py` | PolyForm Strict | 추출 도구는 자체 저작물 |

파생 파일 3종은 `words`(전체의 93%)를 그대로 담고 있어 **파일 전체가**
CC BY-SA 4.0의 적용을 받습니다. `spec/hanja-dict.json` 에서 재생성됩니다.

### `chars` 필드는 왜 다른가

음절→한자 매핑(474 음절 / 5,208 자)은 위키낱말사전이 아니라 다음에서
만들었습니다.

- **KS X 1001** 한자 4,888자 — 한국 산업표준(공개 규격, 문자 목록 자체는
  저작물성이 없는 사실 데이터)
- **Unicode Unihan Database** `kHangul` 속성 — 한국 한자음
  (Unicode License, 사실상 제약 없음)
- 정렬 가중치 — 본 프로젝트 자체 판단

따라서 `chars` 는 CC BY-SA 의무 대상이 아닙니다. 다만 현재
`hanja-dict.json` 한 파일에 `words` 와 함께 들어 있으므로, 파일 단위로
받아 쓸 때는 CC BY-SA 4.0 조건을 따르는 편이 안전합니다.

## 재사용 시 지켜야 할 것

사전 데이터를 가져다 쓰려면 (상업적 이용도 **가능**합니다):

1. **출처 표기** — 한국어 위키낱말사전(<https://ko.wiktionary.org>)에서
   파생되었음을 밝히고, 링크를 제공할 것
2. **변경 사실 명시** — 원본을 가공했음을 표시할 것
   (본 프로젝트는 표제어→한자 매핑 추출, 동음이의어 순위 결정,
   1글자 표제어 389개 제거, 수동 병합 227건, Unihan 음가 전수 검수를 수행)
3. **동일 조건 배포** — 2차 저작물도 CC BY-SA 4.0으로 배포할 것
4. **추가 제약 금지** — 위 권리를 제한하는 조건을 붙이지 말 것

라이선스 전문: <https://creativecommons.org/licenses/by-sa/4.0/legalcode>

## 출처 표기 예시

```
이 소프트웨어는 한국어 위키낱말사전(https://ko.wiktionary.org)에서
파생된 한자 사전 데이터를 포함하며, 해당 데이터는 CC BY-SA 4.0
(https://creativecommons.org/licenses/by-sa/4.0/) 으로 배포됩니다.
원본을 가공하였습니다.
```

## 사전을 뺀 배포

한자 변환 기능이 필요 없다면 아래 파일을 제거하고 코드만 배포할 수 있으며,
이 경우 CC BY-SA 의무가 발생하지 않습니다.

```
spec/hanja-dict.json
ime/core/hanja.js
ime/native/hanja.hpp
ime/android/src/main/java/com/cuime/HanjaDict.java
```

코어 조합 엔진(`cheonjiin.js/.hpp/.java/.py/.swift`)과 펌웨어는 사전에
의존하지 않습니다. 셸에서 한자 키(漢) 동작만 비활성화하면 됩니다.
