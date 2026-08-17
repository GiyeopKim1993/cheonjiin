/* 천지인 펌웨어 HAL — 하드웨어 추상화 계층
 *
 * 이식 방법: 이 헤더의 함수들만 새 보드용으로 구현하면 된다.
 * 코어(cuime.c)와 앱(cuime_app.c)은 손대지 않는다.
 *
 *   ┌─────────────────────────────────────────┐
 *   │ app   cuime_app.c  — 정책·타이머·레이아웃  │  하드웨어 무관
 *   ├─────────────────────────────────────────┤
 *   │ core  cuime.c      — 한글 조합 상태머신    │  하드웨어 무관
 *   ├─────────────────────────────────────────┤
 *   │ HAL   cuime_hal.h  — 이 파일 (인터페이스)  │  ← 보드마다 구현
 *   ├─────────────────────────────────────────┤
 *   │ port  esp32 / stm32 / rp2040 / sim ...   │
 *   └─────────────────────────────────────────┘
 *
 * 구현해야 할 것은 5가지뿐이다:
 *   1. 키 스캔      hal_keys_scan()
 *   2. 시간         hal_millis()
 *   3. 출력(전송)   hal_out_*()      — HID(BT/USB) 또는 UART 등
 *   4. 저장(설정)   hal_nvs_*()      — 없으면 no-op 가능
 *   5. 표시(선택)   hal_ui_*()       — 없으면 no-op 가능
 */
#ifndef CUIME_HAL_H
#define CUIME_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "../core/cuime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 1. 키 입력 ────────────────────────────────────────────
 * 16키 비트마스크. bit N = cuime_key_t N 이 눌린 상태.
 * 디바운스는 HAL 이 책임진다(하드웨어마다 특성이 다르므로).
 */
typedef uint32_t hal_keymask_t;

/** 현재 눌려 있는 키 비트마스크를 반환 */
hal_keymask_t hal_keys_scan(void);

/** 키 스캔 초기화 (GPIO/매트릭스 설정) */
void hal_keys_init(void);


/* ── 2. 시간 ── */

/** 부팅 후 경과 밀리초 (오버플로 허용 — 코어는 차이만 사용) */
uint32_t hal_millis(void);

/** 협조적 양보. RTOS 없으면 no-op 또는 WFI */
void hal_delay_ms(uint32_t ms);


/* ── 3. 출력 ──────────────────────────────────────────────
 * 전송 방식은 보드가 정한다:
 *   - BT/USB HID  : 두벌식 키코드로 변환해 전송 (호스트 IME가 조합)
 *   - BLE/UART    : UTF-8 그대로
 *   - 자체 디스플레이 : 화면에 직접
 */
typedef enum {
    HAL_OUT_HID_DUBEOLSIK = 0,  /* 두벌식 키코드 (기본 — 호스트 IME 사용) */
    HAL_OUT_UNICODE_TEXT        /* UTF-8 문자열 직접 */
} hal_out_mode_t;

void hal_out_init(void);

/** 현재 출력 모드 */
hal_out_mode_t hal_out_mode(void);

/** ASCII 문자 1개를 HID 키로 전송. shift 는 대문자/기호용 */
void hal_out_ascii(char c, bool shift);

/** 유니코드 코드포인트 전송 (HAL_OUT_UNICODE_TEXT 모드) */
void hal_out_unicode(uint32_t codepoint);

/** 특수키 전송 */
typedef enum {
    HAL_KEYCODE_BACKSPACE = 0,
    HAL_KEYCODE_ENTER,
    HAL_KEYCODE_SPACE,
    HAL_KEYCODE_LEFT,
    HAL_KEYCODE_RIGHT,
    HAL_KEYCODE_HANJA        /* 한/영 또는 한자 키 */
} hal_keycode_t;

void hal_out_special(hal_keycode_t k);

/** 호스트 연결 여부 (BT/USB). 항상 true 로 둬도 무방 */
bool hal_out_connected(void);


/* ── 4. 설정 저장 (선택) ── */

/** 키/값 저장. 미지원이면 false 반환해도 동작에 지장 없음 */
bool hal_nvs_set_u32(const char *key, uint32_t val);
bool hal_nvs_get_u32(const char *key, uint32_t *val);


/* ── 5. 사용자 표시 (선택) ── */

/** 조합 중 글자 표시. 디스플레이 없으면 no-op */
void hal_ui_preedit(uint32_t codepoint);

/** 레이아웃 변경 표시 (LED/화면) */
void hal_ui_layout(cuime_layout_t layout);

/** 한자 후보 표시. list 는 코드포인트 배열 */
void hal_ui_candidates(const uint32_t *list, uint8_t count, uint8_t selected);

/** 경고 표시 (M1 전송 가드 등) */
void hal_ui_warn(const char *msg);


/* ── 보드 공통 진입점 ── */

/** 보드 초기화 (클럭·전원·주변장치). port 가 제공 */
void hal_board_init(void);

#ifdef __cplusplus
}
#endif
#endif /* CUIME_HAL_H */
