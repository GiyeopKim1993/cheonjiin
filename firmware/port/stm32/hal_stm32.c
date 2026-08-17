/* HAL 포트: STM32 (USB HID 키보드)
 *
 * 대상: STM32F1/F4/L4 등 USB Device 지원 계열 (STM32 HAL / CubeMX 생성 코드)
 * 출력: USB HID Keyboard — 유선 연결. 두벌식 키코드 전송.
 *
 * 필요한 CubeMX 설정:
 *   - USB_DEVICE: Human Interface Device Class (HID)
 *   - GPIO: 행 4개 출력(Push-Pull), 열 4개 입력(Pull-up)
 *   - SysTick 1ms (기본)
 *
 * ESP32 포트와 다른 부분은 3곳뿐이다: GPIO API, 시간 API, HID 전송 API.
 * 나머지(키맵·디바운스·키코드 변환)는 동일 로직이다.
 */
#include "../../hal/cuime_hal.h"

#ifndef CUIME_PORT_STUB
  #include "main.h"                 /* CubeMX 생성 */
  #include "usbd_hid.h"
  extern USBD_HandleTypeDef hUsbDeviceFS;
#endif

#define ROW_COUNT 4
#define COL_COUNT 4

#ifndef CUIME_PORT_STUB
static GPIO_TypeDef *ROW_PORT[ROW_COUNT] = { GPIOB, GPIOB, GPIOB, GPIOB };
static const uint16_t ROW_PIN[ROW_COUNT] = { GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3 };
static GPIO_TypeDef *COL_PORT[COL_COUNT] = { GPIOA, GPIOA, GPIOA, GPIOA };
static const uint16_t COL_PIN[COL_COUNT] = { GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7 };
#endif

static const cuime_key_t KEYMAP[ROW_COUNT][COL_COUNT] = {
    { CUIME_KEY_1,    CUIME_KEY_2, CUIME_KEY_3,     CUIME_KEY_BACK  },
    { CUIME_KEY_4,    CUIME_KEY_5, CUIME_KEY_6,     CUIME_KEY_ENTER },
    { CUIME_KEY_7,    CUIME_KEY_8, CUIME_KEY_9,     CUIME_KEY_HANJA },
    { CUIME_KEY_LEFT, CUIME_KEY_0, CUIME_KEY_RIGHT, CUIME_KEY_SPACE },
};

#define DEBOUNCE_SAMPLES 2
static hal_keymask_t s_stable, s_last_raw;
static uint8_t s_stable_cnt;

void hal_keys_init(void)
{
    /* GPIO 초기화는 CubeMX 생성 MX_GPIO_Init() 이 담당 */
    s_stable = 0; s_last_raw = 0; s_stable_cnt = 0;
}

static hal_keymask_t scan_raw(void)
{
    hal_keymask_t m = 0;
#ifndef CUIME_PORT_STUB
    for (int r = 0; r < ROW_COUNT; r++) {
        HAL_GPIO_WritePin(ROW_PORT[r], ROW_PIN[r], GPIO_PIN_RESET);
        for (volatile int d = 0; d < 50; d++) { }        /* 안정화 */
        for (int c = 0; c < COL_COUNT; c++) {
            if (HAL_GPIO_ReadPin(COL_PORT[c], COL_PIN[c]) == GPIO_PIN_RESET)
                m |= (hal_keymask_t)1u << KEYMAP[r][c];
        }
        HAL_GPIO_WritePin(ROW_PORT[r], ROW_PIN[r], GPIO_PIN_SET);
    }
#endif
    return m;
}

hal_keymask_t hal_keys_scan(void)
{
    hal_keymask_t raw = scan_raw();
    if (raw == s_last_raw) {
        if (s_stable_cnt < DEBOUNCE_SAMPLES) s_stable_cnt++;
        if (s_stable_cnt >= DEBOUNCE_SAMPLES) s_stable = raw;
    } else { s_last_raw = raw; s_stable_cnt = 0; }
    return s_stable;
}

uint32_t hal_millis(void)
{
#ifdef CUIME_PORT_STUB
    return 0;
#else
    return HAL_GetTick();
#endif
}

void hal_delay_ms(uint32_t ms)
{
#ifndef CUIME_PORT_STUB
    HAL_Delay(ms);
#else
    (void)ms;
#endif
}

void hal_out_init(void) { }
hal_out_mode_t hal_out_mode(void) { return HAL_OUT_HID_DUBEOLSIK; }

bool hal_out_connected(void)
{
#ifdef CUIME_PORT_STUB
    return false;
#else
    return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED;
#endif
}

static uint8_t ascii_to_hid(char c, bool *need_shift)
{
    *need_shift = false;
    if (c >= 'a' && c <= 'z') return (uint8_t)(0x04 + (c - 'a'));
    if (c >= 'A' && c <= 'Z') { *need_shift = true; return (uint8_t)(0x04 + (c - 'A')); }
    if (c >= '1' && c <= '9') return (uint8_t)(0x1E + (c - '1'));
    switch (c) {
        case '0': return 0x27;  case ' ': return 0x2C;
        case '-': return 0x2D;  case '=': return 0x2E;
        case '[': return 0x2F;  case ']': return 0x30;
        case ';': return 0x33;  case '\'': return 0x34;
        case ',': return 0x36;  case '.': return 0x37;
        case '/': return 0x38;
        case '!': *need_shift = true; return 0x1E;
        case '?': *need_shift = true; return 0x38;
        case ':': *need_shift = true; return 0x33;
        case '"': *need_shift = true; return 0x34;
        case '(': *need_shift = true; return 0x26;
        case ')': *need_shift = true; return 0x27;
        default: return 0;
    }
}

static void hid_send(uint8_t keycode, bool shift)
{
#ifndef CUIME_PORT_STUB
    uint8_t report[8] = { (uint8_t)(shift ? 0x02 : 0x00), 0, keycode, 0, 0, 0, 0, 0 };
    USBD_HID_SendReport(&hUsbDeviceFS, report, 8);
    HAL_Delay(2);
    uint8_t rel[8] = {0};
    USBD_HID_SendReport(&hUsbDeviceFS, rel, 8);
    HAL_Delay(2);
#else
    (void)keycode; (void)shift;
#endif
}

void hal_out_ascii(char c, bool shift)
{
    bool need = false;
    uint8_t kc = ascii_to_hid(c, &need);
    if (kc) hid_send(kc, shift || need);
}

void hal_out_unicode(uint32_t cp) { (void)cp; }   /* HID 불가 */

void hal_out_special(hal_keycode_t k)
{
    switch (k) {
        case HAL_KEYCODE_BACKSPACE: hid_send(0x2A, false); break;
        case HAL_KEYCODE_ENTER:     hid_send(0x28, false); break;
        case HAL_KEYCODE_SPACE:     hid_send(0x2C, false); break;
        case HAL_KEYCODE_LEFT:      hid_send(0x50, false); break;
        case HAL_KEYCODE_RIGHT:     hid_send(0x4F, false); break;
        case HAL_KEYCODE_HANJA:     hid_send(0x8A, false); break;
    }
}

/* 내장 플래시 EEPROM 에뮬레이션이 없으면 미지원 반환 (HAL 계약상 허용) */
bool hal_nvs_set_u32(const char *key, uint32_t val) { (void)key; (void)val; return false; }
bool hal_nvs_get_u32(const char *key, uint32_t *val) { (void)key; (void)val; return false; }

void hal_ui_preedit(uint32_t cp) { (void)cp; }
void hal_ui_layout(cuime_layout_t l) { (void)l; }
void hal_ui_candidates(const uint32_t *l, uint8_t n, uint8_t s) { (void)l; (void)n; (void)s; }
void hal_ui_warn(const char *m) { (void)m; }

/* STUB 빌드에서는 스캔 본문이 컴파일되지 않아 매트릭스 상수가 미사용이 된다.
 * 실기기 빌드에서는 전부 사용된다. */
static void suppress_unused(void) { (void)KEYMAP; (void)suppress_unused; }

void hal_board_init(void) { suppress_unused(); hal_keys_init(); hal_out_init(); }
