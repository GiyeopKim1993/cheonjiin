# 물리 키보드 브리지

천지인 물리 키보드를 IME에 연결한다. **디바이스는 조합하지 않는다** —
키가 눌리고 떨어진 사실만 보내고, 조합·멀티탭 타이머·롱프레스 판정·
프리에딧 표시·한자 후보는 전부 호스트 IME가 한다.

```
[키보드]  키 눌림/뗌  ──USB HID──>  [브리지]  ──>  [IME 셸]  ──>  화면
 6~7KB 펌웨어                      키 이름 변환      조합·한자
```

## 왜 이렇게 나눴나

디바이스에서 조합을 끝내면 세 가지가 깨진다:

| | 디바이스 조합 | 호스트 조합 (현재) |
|---|---|---|
| 조합 중 글자 표시 | ❌ LED뿐 | ✅ 밑줄 프리에딧 |
| 한자 변환 | ❌ 사전 794KB > 플래시 512KB | ✅ 후보 33,666개 |
| 조합 횟수 | 디바이스 + 호스트 = 2번 | 1번 |

## 왜 CDC가 아니라 커스텀 HID인가

CDC-ACM(가상 시리얼)은 COM 포트로 뜬다:

- 포트 번호가 꽂는 순서·포트마다 바뀐다 (COM3 → COM17)
- 다른 프로그램이 선점하면 IME가 못 연다
- 키보드가 시리얼 포트를 노출하는 것 자체가 이상하다
- Windows에서 `usbser.sys` + IAD를 틀리면 포트가 아예 안 뜬다

커스텀 HID는 VID/PID/UsagePage로 고정 식별되고, 3개 OS 모두 드라이버가 없다.
엔드포인트도 3개만 써서 F411의 4개 한계 안에 여유가 남는다(CDC+HID는 딱 4개).

## 구조

| 파일 | 역할 |
|---|---|
| `bridge.js` | 프로토콜 해석 + 셸 API 호출. **전송 계층을 모른다** |
| `transport-hid.js` | USB HID 전송 (node-hid). `{on, send, close}` 제공 |
| `cuime-device.js` | CLI 데몬 |
| `test-proto-sync.js` | **C 헤더 ↔ JS 상수 대조** |
| `test-bridge.js` | 조합·멀티탭·한자·패킷유실 |
| `test-transport.js` | 복합장치 인터페이스 선택 |

`bridge.js`가 전송 계층을 모르는 덕에 BLE를 붙일 때 `{on, send, close}`만
구현하면 되고, 실기기 없이도 전 로직을 검증할 수 있다.

## 사용법

```bash
npm i node-hid                    # 실제 장치용
node cuime-device.js              # 조합 결과를 화면에 표시
node cuime-device.js --list       # 연결된 HID 장치 목록
node cuime-device.js --demo       # 장치 없이 시연 (node-hid 불필요)
```

`--demo` 출력 — 확정분과 조합 중이 구분된다:

```
[ㅅ] → [ㅎ] → [히] → [하] → [한] → 한[ㄱ] → 한[그] → 한[근] → 한[글]
```

## 프로토콜

32바이트 고정 리포트. 정의는 `firmware/hal/cuime_rawhid.h`가 **정본**이고,
`test-proto-sync.js`가 C 헤더를 파싱해 JS 상수와 대조한다.

같은 숫자를 두 곳에 적으면 언젠가 어긋나는데, 어긋나도 컴파일은 되고
테스트도 통과하며 실기기에서만 조용히 깨진다. 그래서 기계적으로 막는다.

| 방향 | 메시지 | 내용 |
|---|---|---|
| 장치→호스트 | `EV_KEY` | 키 번호, 눌림/뗌, 타임스탬프 |
| | `EV_HELLO` | 프로토콜 버전, 키 개수, 펌웨어 이름 |
| | `EV_PONG` | 하트비트 응답 |
| 호스트→장치 | `CMD_HELLO` | IME 접속 → 장치가 RAW 모드로 전환 |
| | `CMD_BYE` | 정상 종료 → 즉시 두벌식 폴백 |
| | `CMD_PING` | 하트비트 (1초 주기) |
| | `CMD_LED` | LED 제어 |

타임스탬프를 함께 보내는 이유: USB 전송 지연이 흔들려도 호스트가 멀티탭
간격을 정확히 잴 수 있다. 도착 시각으로 재면 오판이 생긴다.

## 두벌식 폴백

장치는 커스텀 HID와 **부트 키보드를 동시에** 노출한다. IME가 없으면
일반 키보드로 동작하므로, 남의 컴퓨터에 꽂아도 쓸 수 있다.

```
부팅: 부트 키보드 활성 (IME 없어도 입력 가능)
  ↓ IME가 CMD_HELLO 전송
RAW 모드: 부트 키보드 침묵, 키 이벤트만 전송
  ↓ CMD_BYE 또는 하트비트 3초 끊김
부트 키보드 복귀
```

하트비트가 필요한 이유: 프로세스가 강제 종료되면 `BYE`를 못 보낸다.
그대로 RAW 모드에 남으면 키를 눌러도 아무 반응 없는 **먹통 키보드**가 된다.

## 플랫폼별 주의

### macOS — 입력 모니터링 권한

Catalina부터 HID 접근에 권한이 필요하다. 없으면 `IOHIDDeviceOpen`이
`kIOReturnNotPermitted`로 실패한다. 키보드가 아닌 장치도 마찬가지다.

> 시스템 설정 → 개인정보 보호 및 보안 → **입력 모니터링** → 앱 허용

권한이 없으면 브리지가 원인을 명시하고 종료한다. 그동안 장치는 두벌식
폴백으로 동작하므로 입력 자체가 막히지는 않는다.

usage page `0xFF60`은 macOS에서 문제없다(QMK가 쓰는 값). 단 리포트
디스크립터에서 **`Usage`가 0이거나 `Collection`이 Application(0x01)이
아니면 macOS가 인터페이스를 통째로 무시한다.** `test_rawhid.c`가 이
조건을 실제 파싱으로 검증한다.

### Linux — hidraw 권한

```
# /etc/udev/rules.d/99-cuime.rules
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="1209", ATTRS{idProduct}=="ce01", MODE="0666"
```

```bash
sudo udevadm control --reload-rules && sudo udevadm trigger
```

### Windows

드라이버 불필요. 다만 복합장치라 같은 VID/PID로 인터페이스가 여러 개
(`MI_00`/`MI_01`) 잡힌다. **VID/PID만으로 열면 부트 키보드를 잡을 수 있다** —
반드시 usagePage까지 걸러야 한다. `transport-hid.js`가 처리한다.

## 검증

```bash
node test-proto-sync.js   # C 헤더 ↔ JS 상수
node test-bridge.js       # 조합 전 경로
node test-transport.js    # 장치 선택
```

실기기 없이 돌아간다. 가짜 전송 계층이 펌웨어 리포트를 흉내 내고,
IME 셸까지 통과시켜 실제로 한글이 조합되는지 확인한다.

키 이름 배열은 손으로 맞추지 않고 **`firmware/core/cuime.h`의 enum을
파싱해 대조**한다. 배열을 일부러 어긋내면 테스트가 잡는 것까지 확인했다.
