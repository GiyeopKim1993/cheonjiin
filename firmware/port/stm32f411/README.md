# STM32F411 실기기 데모

천지인 IME 를 **실제 보드에서 돌리는** 최소 구성. CubeMX·HAL 라이브러리·CMSIS
없이 이 디렉터리 파일만으로 빌드된다(레지스터 직접 제어).

- 대상: **STM32F411CEU6 "Black Pill"** / Nucleo-F411RE
- 코어 Cortex-M4F, Flash 512KB @ `0x08000000`, SRAM 128KB @ `0x20000000`
- 산출물 크기: **6,136 B** (플래시의 1.17%) / RAM 1,184 B (0.90%)

---

## 1. 배선

4×4 매트릭스. **다이오드 불필요**(1키 입력 기준. 2키 이상 동시 입력을 쓰려면
각 스위치에 1N4148 을 행→열 방향으로 넣는다).

```
        열0     열1     열2     열3
        PA4     PA5     PA6     PA7
행0 PB0  ㅣ      ㆍ      ㅡ      ⌫
행1 PB1 ㄱㅋ    ㄴㄹ    ㄷㅌ      ↵
행2 PB2 ㅂㅍ    ㅅㅎ    ㅈㅊ      漢
행3 PB3  ◀     ㅇㅁ     ▶       ␣
```

> 열을 PA0~PA3 이 아니라 **PA4~PA7** 로 둔 이유: PA2 는 USART2_TX 라
> UART 출력 모드에서 열2 와 충돌한다. 기존 `port/stm32` 와도 동일한 배치다.

| 신호 | 핀 | 설정 |
|---|---|---|
| 행 0~3 | PB0~PB3 | 출력 push-pull, 평소 High, 스캔 시 해당 행만 Low |
| 열 0~3 | PA4~PA7 | 입력 **풀업**, 눌리면 Low |
| LED | PC13 | 출력, **active-low**(Low=점등). 조합 중 점등 |
| UART TX | PA2 | AF7, 115200 8N1 |
| USB | PA11(D−) / PA12(D+) | HID 모드일 때 |

## 2. 빌드

```bash
./build.sh                      # 기본: HSI 100MHz + UART 출력
USE_HSE=1 ./build.sh            # HSE 25MHz -> 96MHz (USB 쓰려면 필수)
USB_HID=1 ./build.sh            # USB HID 두벌식 모드
```

`arm-none-eabi-gcc` 가 있으면 그것을, 없으면 **zig** 를 자동으로 쓴다
(`pip install --user ziglang`). 산출물은 `build/cuime-f411.{elf,bin,hex}`.

빌드는 3단계를 모두 통과해야 성공한다:
1. 크로스 컴파일·링크
2. **정적 검증** (`tools/verify_f411.py`) — 벡터 테이블 위치, 초기 SP,
   Reset 의 thumb 비트, SysTick 연결, 용량, 심볼 잔존
3. **로직 검증** (`test/test_f411.c`) — 아래 참조

## 3. 플래싱

```bash
st-flash write build/cuime-f411.bin 0x08000000
# 또는 (BOOT0=1 로 DFU 진입 후)
dfu-util -a 0 -s 0x08000000:leave -D build/cuime-f411.bin
# 또는
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build/cuime-f411.elf verify reset exit"
```

## 4. 동작 확인

UART 모드: PA2 를 USB-TTL 어댑터에 물리고 **115200 8N1**.

```bash
screen /dev/ttyUSB0 115200      # 또는 minicom -D /dev/ttyUSB0 -b 115200
```

부팅하면 `천지인 IME (STM32F411)` 배너가 뜬다. 키를 누르면 조합된 한글이
UTF-8 로 그대로 나온다. 조합 중에는 PC13 LED 가 켜진다.

예) `ㄱ` → `ㅣ` → `ㆍ` → `␣` 를 누르면 `가 ` 가 출력된다.

## 5. 실기기 없이 하는 검증

이 포트의 로직은 **x86 에서 그대로 실행해** 검증한다. `CUIME_F411_HOSTTEST`
를 정의하면 `stm32f411_regs.h` 대신 `stm32f411_hosttest.h`(가짜 레지스터)가
잡히고, 하드웨어 접근만 가짜가 되며 매핑·디바운스·인코딩 로직은 원본 그대로
돌아간다.

검사 항목 (`test/test_f411.c`):
- 키 매트릭스 16키 매핑 (중복·누락 없음)
- 디바운스: 변화 후 2회 연속 일치해야 확정 / 채터링 중 stable 유지
- UTF-8 인코딩 1·2·3바이트 (`가` `A` `é` `힣`)
- HID 키코드 변환 (shift 포함)
- **엔드투엔드**: 매트릭스 입력 → 조합 → 출력
  - `ㄱㅣㆍ␣` → `가 `
  - `ㅅㅅ`(멀티탭 ㅎ)`ㅣㆍㄴ␣` → `한 `
  - 역조합 `⌫`: `가` → `기` (획 단위 취소)
  - HID 모드: `가` → 두벌식 `r`,`k`,`space` = `0x15,0x0E,0x2C`

## 6. 클럭 구성

| 설정 | SYSCLK | USB 48MHz | 용도 |
|---|---|---|---|
| 기본 (HSI 16MHz) | 100MHz (M8 N100 P2) | ❌ | UART 데모 |
| `USE_HSE=1` (HSE 25MHz) | 96MHz (M25 N192 P2 Q4) | ✅ | USB HID |

> **HSI 로는 USB 를 못 쓴다.** USB FS 는 정확히 48MHz 를 요구하는데 HSI 는
> ±1% 오차라 규격을 못 맞춘다. USB HID 를 쓸 거면 `USE_HSE=1` 이 필수다.

FLASH_ACR = 3 wait states + 프리페치 + I/D 캐시. AHB /1, APB1 /2(50MHz), APB2 /1.

## 7. 파일 구성

| 파일 | 역할 |
|---|---|
| `hal_stm32f411.c` | HAL 5영역 구현 (keys/time/out/nvs/ui) |
| `stm32f411_regs.h` | 레지스터·비트 정의 (RM0383 기준) |
| `stm32f411_hosttest.h` | 호스트 검증용 가짜 레지스터 |
| `startup_stm32f411.c` | 벡터 테이블, `.data`/`.bss` 초기화, FPU 활성화 |
| `stm32f411.ld` | 링커 스크립트 |
| `main_stm32f411.c` | `hal_board_init()` + 5ms 폴링 루프 |
| `freestanding_libc.c` | libc 없는 환경용 `memset`/`strlen` 등 |
| `build.sh` | 빌드 + 정적 검증 + 로직 검증 |

## 8. 미구현 (범위 밖)

- **USB 스택**: HID 모드는 `cuime_usb_hid_send()` / `cuime_usb_hid_ready()`
  weak 훅만 제공한다. TinyUSB 등을 별도로 링크해 구현해야 실제로 전송된다.
- **설정 저장**: 내장 플래시 EEPROM 에뮬레이션 미구현 → `hal_nvs_*` 는
  `false` 반환 (HAL 계약상 허용).
- **후보 표시**: 디스플레이가 없어 한자 후보는 LED 피드백만.
- **실기기 실측**: 보드가 없어 **아직 물리 검증은 하지 않았다.** 위 검증은
  전부 정적 분석 + 호스트 실행이다.

## 9. 다른 보드로 옮기기

`hal_stm32f411.c` 의 상단 3개만 고치면 대개 끝난다:

```c
static const uint8_t ROW_PIN[ROW_COUNT] = { 0, 1, 2, 3 };   /* PB0..PB3 */
static const uint8_t COL_PIN[COL_COUNT] = { 4, 5, 6, 7 };   /* PA4..PA7 */
static const cuime_key_t KEYMAP[ROW_COUNT][COL_COUNT] = { ... };
```

다른 STM32 계열(F103/F401/F446 등)이면 `stm32f411_regs.h` 의 베이스 주소와
클럭 설정(PLL 계수, 플래시 wait state)을 해당 레퍼런스 매뉴얼에 맞춰 조정한다.
전혀 다른 MCU 는 `firmware/README.md` 의 이식 가이드를 따른다.
