/* HAL 포트: ESP32 (BLE HID 키보드)
 *
 * 대상: ESP32 / ESP32-S3 / ESP32-C3 (ESP-IDF v5.x)
 * 출력: BLE HID Keyboard — 호스트(PC/폰)가 일반 블루투스 키보드로 인식.
 *       두벌식 키코드를 보내면 호스트의 한글 IME가 음절로 조합한다.
 *
 * 빌드: ESP-IDF 컴포넌트로 등록 (CMakeLists.txt 참조)
 *
 * 이 파일은 ESP-IDF 헤더가 있어야 컴파일된다. 호스트에서는
 * CUIME_PORT_STUB 로 인터페이스 정합성만 확인한다.
 */
#include "../../hal/cuime_hal.h"

#ifndef CUIME_PORT_STUB
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "driver/gpio.h"
  #include "esp_timer.h"
  #include "nvs_flash.h"
  #include "nvs.h"
  #include "esp_hidd_prf_api.h"     /* BLE HID */
#endif

/* ── 키 매트릭스 배선 (보드에 맞게 수정) ──
 * 4×4 매트릭스: 행 4개 출력, 열 4개 입력(풀업)
 * 물리 배치는 docs/00 §2 확정 배치를 따른다.
 */
#define ROW_COUNT 4
#define COL_COUNT 4

static const int ROW_PINS[ROW_COUNT] = { 4, 5, 6, 7 };
static const int COL_PINS[COL_COUNT] = { 15, 16, 17, 18 };

/* 매트릭스 (행,열) -> 논리 키 (docs/00 §2)
 *    ㅣ    ㆍ    ㅡ    ⌫
 *   ㄱㅋ  ㄴㄹ  ㄷㅌ   ↵
 *   ㅂㅍ  ㅅㅎ  ㅈㅊ   漢
 *    ◀   ㅇㅁ   ▶    ␣
 */
static const cuime_key_t KEYMAP[ROW_COUNT][COL_COUNT] = {
    { CUIME_KEY_1,    CUIME_KEY_2, CUIME_KEY_3,     CUIME_KEY_BACK  },
    { CUIME_KEY_4,    CUIME_KEY_5, CUIME_KEY_6,     CUIME_KEY_ENTER },
    { CUIME_KEY_7,    CUIME_KEY_8, CUIME_KEY_9,     CUIME_KEY_HANJA },
    { CUIME_KEY_LEFT, CUIME_KEY_0, CUIME_KEY_RIGHT, CUIME_KEY_SPACE },
};

/* 디바운스: 5ms 간격 2회 연속 일치해야 확정 */
#define DEBOUNCE_SAMPLES 2
static hal_keymask_t s_stable, s_last_raw;
static uint8_t s_stable_cnt;

void hal_keys_init(void)
{
#ifndef CUIME_PORT_STUB
    for (int r = 0; r < ROW_COUNT; r++) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << ROW_PINS[r],
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io);
        gpio_set_level(ROW_PINS[r], 1);
    }
    for (int c = 0; c < COL_COUNT; c++) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << COL_PINS[c],
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io);
    }
#endif
    s_stable = 0; s_last_raw = 0; s_stable_cnt = 0;
}

static hal_keymask_t scan_raw(void)
{
    hal_keymask_t m = 0;
#ifndef CUIME_PORT_STUB
    for (int r = 0; r < ROW_COUNT; r++) {
        gpio_set_level(ROW_PINS[r], 0);            /* 해당 행만 LOW */
        esp_rom_delay_us(3);                        /* 안정화 */
        for (int c = 0; c < COL_COUNT; c++) {
            if (gpio_get_level(COL_PINS[c]) == 0)   /* 풀업 -> 눌리면 LOW */
                m |= (hal_keymask_t)1u << KEYMAP[r][c];
        }
        gpio_set_level(ROW_PINS[r], 1);
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
    } else {
        s_last_raw = raw; s_stable_cnt = 0;
    }
    return s_stable;
}

uint32_t hal_millis(void)
{
#ifdef CUIME_PORT_STUB
    return 0;
#else
    return (uint32_t)(esp_timer_get_time() / 1000);
#endif
}

void hal_delay_ms(uint32_t ms)
{
#ifndef CUIME_PORT_STUB
    vTaskDelay(pdMS_TO_TICKS(ms));
#else
    (void)ms;
#endif
}

/* ── BLE HID 출력 ── */

static bool s_connected = false;

void hal_out_init(void)
{
#ifndef CUIME_PORT_STUB
    esp_hidd_profile_init();
#endif
}

hal_out_mode_t hal_out_mode(void) { return HAL_OUT_HID_DUBEOLSIK; }
bool hal_out_connected(void) { return s_connected; }

/* USB HID 키코드 (HID Usage Table 1.12, Keyboard/Keypad Page) */
static uint8_t ascii_to_hid(char c, bool *need_shift)
{
    *need_shift = false;
    if (c >= 'a' && c <= 'z') return (uint8_t)(0x04 + (c - 'a'));
    if (c >= 'A' && c <= 'Z') { *need_shift = true; return (uint8_t)(0x04 + (c - 'A')); }
    if (c >= '1' && c <= '9') return (uint8_t)(0x1E + (c - '1'));
    switch (c) {
        case '0': return 0x27;
        case ' ': return 0x2C;
        case '-': return 0x2D;
        case '=': return 0x2E;
        case '[': return 0x2F;
        case ']': return 0x30;
        case ';': return 0x33;
        case '\'': return 0x34;
        case ',': return 0x36;
        case '.': return 0x37;
        case '/': return 0x38;
        case '!': *need_shift = true; return 0x1E;
        case '?': *need_shift = true; return 0x38;
        case ':': *need_shift = true; return 0x33;
        case '"': *need_shift = true; return 0x34;
        case '(': *need_shift = true; return 0x26;
        case ')': *need_shift = true; return 0x27;
        case '@': *need_shift = true; return 0x1F;
        case '#': *need_shift = true; return 0x20;
        default: return 0;
    }
}

static void hid_send(uint8_t keycode, bool shift)
{
#ifndef CUIME_PORT_STUB
    uint8_t mod = shift ? 0x02 : 0x00;              /* Left Shift */
    uint8_t report[8] = { mod, 0, keycode, 0, 0, 0, 0, 0 };
    esp_hidd_send_keyboard_value(0, mod, &report[2], 6);
    /* 키 뗌 */
    uint8_t rel[8] = {0};
    esp_hidd_send_keyboard_value(0, 0, &rel[2], 6);
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

void hal_out_unicode(uint32_t cp)
{
    /* HID 키보드는 유니코드를 직접 보낼 수 없다.
     * 두벌식 모드가 기본이므로 여기 오면 무시한다.
     * (UART/BLE-UART 포트에서는 UTF-8 바이트를 전송하도록 구현) */
    (void)cp;
}

void hal_out_special(hal_keycode_t k)
{
    switch (k) {
        case HAL_KEYCODE_BACKSPACE: hid_send(0x2A, false); break;
        case HAL_KEYCODE_ENTER:     hid_send(0x28, false); break;
        case HAL_KEYCODE_SPACE:     hid_send(0x2C, false); break;
        case HAL_KEYCODE_LEFT:      hid_send(0x50, false); break;
        case HAL_KEYCODE_RIGHT:     hid_send(0x4F, false); break;
        case HAL_KEYCODE_HANJA:     hid_send(0x8A, false); break;  /* Lang2 (한/영) */
    }
}

/* ── NVS 설정 저장 ── */

bool hal_nvs_set_u32(const char *key, uint32_t val)
{
#ifdef CUIME_PORT_STUB
    (void)key; (void)val; return false;
#else
    nvs_handle_t h;
    if (nvs_open("cuime", NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = (nvs_set_u32(h, key, val) == ESP_OK) && (nvs_commit(h) == ESP_OK);
    nvs_close(h);
    return ok;
#endif
}

bool hal_nvs_get_u32(const char *key, uint32_t *val)
{
#ifdef CUIME_PORT_STUB
    (void)key; (void)val; return false;
#else
    nvs_handle_t h;
    if (nvs_open("cuime", NVS_READONLY, &h) != ESP_OK) return false;
    bool ok = (nvs_get_u32(h, key, val) == ESP_OK);
    nvs_close(h);
    return ok;
#endif
}

/* ── UI (LED 없으면 no-op) ── */
void hal_ui_preedit(uint32_t cp) { (void)cp; }
void hal_ui_layout(cuime_layout_t l) { (void)l; }
void hal_ui_candidates(const uint32_t *list, uint8_t n, uint8_t sel)
{ (void)list; (void)n; (void)sel; }
void hal_ui_warn(const char *msg) { (void)msg; }

/* STUB 빌드에서는 스캔 본문이 컴파일되지 않아 매트릭스 상수가 미사용이 된다.
 * 실기기 빌드에서는 전부 사용된다. */
static void suppress_unused(void) { (void)KEYMAP; (void)ROW_PINS; (void)COL_PINS; }

void hal_board_init(void)
{
#ifndef CUIME_PORT_STUB
    nvs_flash_init();
#endif
    suppress_unused();
    hal_keys_init();
    hal_out_init();
}
