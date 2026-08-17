# 저장소에 올리는 방법

커밋은 이미 만들어져 있습니다. **인증 정보가 없어 제가 직접 푸시할 수는 없습니다.**
아래 중 편한 방법을 쓰세요.

```
커밋   8822704  천지인 IME — 코어 5개 언어 + 5개 플랫폼 + 물리 키보드 펌웨어
파일   131개 (빌드 산출물 제외)
원격   https://github.com/GiyeopKim1993/cheonjiin.git (origin 설정 완료)
```

---

## ⚠️ 먼저: PR은 지금 만들 수 없습니다

`GiyeopKim1993/cheonjiin` 은 **빈 저장소(empty repository)** 입니다.
커밋이 하나도 없으면 기준 브랜치가 없어서 **Pull Request 자체를 만들 수 없습니다.**

순서가 이렇게 되어야 합니다.

1. `main` 을 먼저 푸시해서 저장소를 초기화 (아래 방법 A)
2. 그 다음부터 기능 브랜치 → PR 이 가능해집니다 (아래 방법 C)

---

## 방법 A — 워크스페이스에서 직접 푸시 (가장 간단)

GitHub에서 **Personal Access Token** 을 발급받으세요.
Settings → Developer settings → Personal access tokens → Tokens (classic)
→ Generate new token → `repo` 권한 체크

발급받은 토큰을 저에게 알려주시면 제가 푸시하겠습니다. 또는 직접:

```bash
cd /home/user/cheonjiin
git push https://<TOKEN>@github.com/GiyeopKim1993/cheonjiin.git main
```

> 토큰은 비밀번호와 같습니다. 채팅에 남기고 싶지 않으시면 방법 B를 쓰세요.

---

## 방법 B — 번들 파일 내려받아 로컬에서 푸시 (토큰 노출 없음)

워크스페이스에 `cheonjiin.bundle` (1.3MB) 을 만들어 뒀습니다.
이 파일 하나에 저장소 전체(커밋 이력 포함)가 들어 있습니다.

1. 워크스페이스에서 `cheonjiin.bundle` 다운로드
2. 본인 PC에서:

```bash
git clone cheonjiin.bundle cheonjiin
cd cheonjiin
git remote set-url origin https://github.com/GiyeopKim1993/cheonjiin.git
git push -u origin main
```

번들이 정상인지는 이미 확인했습니다 — 복원본으로 `./run-all-tests.sh` 전체를
돌려 **iOS 포함 전 항목 통과**를 검증했습니다.

---

## 방법 C — 초기화 후 PR 만들기

`main` 이 올라간 뒤에는 정상적으로 PR을 만들 수 있습니다.

```bash
cd cheonjiin
git checkout -b feature/ime-v1
# ... 변경 작업 ...
git commit -am "변경 내용"
git push -u origin feature/ime-v1
```

푸시하면 GitHub가 PR 생성 링크를 출력합니다. 또는
`https://github.com/GiyeopKim1993/cheonjiin/compare/main...feature/ime-v1`

---

## 푸시 후 확인할 것

### 1. CI가 실제로 도는지

`.github/workflows/ci.yml` 에 9개 잡이 있습니다. 푸시하면 자동 실행됩니다.
로컬에서 검증 불가능했던 것들이 여기서 처음 실증됩니다.

- `android` — APK 아티팩트
- `windows-tsf` — MSVC DLL + COM 내보내기 검사
- `ios` — macOS 러너에서 **실제 UIKit arm64 컴파일**
- `firmware` — 실제 `arm-none-eabi-gcc` 크로스 컴파일
- `e2e` — 실제 Chrome 28건

CI가 처음 돌 때 실패할 가능성이 있는 지점은 `README.md` 의 "검증 현황"에
적어뒀습니다. 실패하면 로그를 보여주시면 고치겠습니다.

### 2. 라이선스 표시

GitHub는 `LICENSE` 파일을 자동 인식하는데, PolyForm Strict 는 OSI 승인
라이선스가 아니라 "Unknown license" 로 표시될 수 있습니다. 정상입니다.

`LICENSE-DICT.md` 는 별도로 인식되지 않으므로, 사전 데이터가 CC BY-SA 4.0
이라는 점은 README 에 명시해뒀습니다.

### 3. 저장소 설정 권장값

- **Actions 권한**: Settings → Actions → General → Workflow permissions
  → `Read and write` (아티팩트 업로드에 필요)
- 코드 무단 사용을 막고 싶으시면 저장소를 **private** 으로 두세요.
  public 저장소는 GitHub ToS 상 열람·fork 를 막을 수 없습니다.
