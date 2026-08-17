/* HAL 포트: RP2040 / Raspberry Pi Pico (USB HID, TinyUSB)
 *
 * 대상: Raspberry Pi Pico / Pico W (pico-sdk)
 * 출력: USB HID Keyboard (TinyUSB). 두벌식 키코드 전송.
 *
 * ESP32/STM32 포트와 다른 부분은 GPIO·시간·HID API 3곳뿐이다.
 */
#include "../../hal/cuime_hal.h"

#ifndef CUIME_PORT_STUB
  #include "pico/stdlib.h"
  #include "hardware/gpio.h"
  #include "tusb.h"
  #include "hardware/flash.h"
#endif

#define ROW_COUNT 4
#define COL_COUNT 4

static const uint8_t ROW_PINS[ROW_COUNT] = { 2, 3, 4, 5 };
static const uint8_t COL_PINS[COL_COUNT] = { 6, 7, 8, 9 };

static const cuime_key_t KEYMAP[ROW_COUNT][COL_COUNT] = {
    { CUIME_KEY_1,    CUIME_KEY_2, CUIME_KEY_3,     CUIME_KEY_BACK  },
    { CUIME_KEY_4,    CUIME_KEY_5, CUIME_KEY_6,     CUIME_KEY_ENTER },
    { CUIME_KEY_7,    CUIME_KEY_8, CUIME_KEY_9,     CUIME_KEY_HANJA },
    { CUIME_KEY_LEFT, CUIME_KEY_0, CUIME_KEY_RIGHT, CUIME_KEY_SPACE },
};

#define DEBOUNCE_SAMPLES 2
static hal_keymask_t s_stable, s_last_raw;
static uint8_t s_stable_cnt;

static void suppress_unused(void)
{ (void)KEYMAP; (void)ROW_PINS; (void)COL_PINS; }

void hal_keys_init(void)
{
#ifndef CUIME_PORT_STUB
    for (int r = 0; r < ROW_COUNT; r++) {
        gpio_init(ROW_PINS[r]);
        gpio_set_dir(ROW_PINS[r], GPIO_OUT);
        gpio_put(ROW_PINS[r], 1);
    }
    for (int c = 0; c < COL_COUNT; c++) {
        gpio_init(COL_PINS[c]);
        gpio_set_dir(COL_PINS[c], GPIO_IN);
        gpio_pull_up(COL_PINS[c]);
    }
#endif
    s_stable = 0; s_last_raw = 0; s_stable_cnt = 0;
}

static hal_keymask_t scan_raw(void)
{
    hal_keymask_t m = 0;
#ifndef CUIME_PORT_STUB
    for (int r = 0; r < ROW_COUNT; r++) {
        gpio_put(ROW_PINS[r], 0);
        busy_wait_us(3);
        for (int c = 0; c < COL_COUNT; c++)
            if (!gpio_get(COL_PINS[c]))
                m |= (hal_keymask_t)1u << KEYMAP[r][c];
        gpio_put(ROW_PINS[r], 1);
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
    return to_ms_since_boot(get_absolute_time());
#endif
}

void hal_delay_ms(uint32_t ms)
{
#ifndef CUIME_PORT_STUB
    sleep_ms(ms);
#else
    (void)ms;
#endif
}

void hal_out_init(void)
{
#ifndef CUIME_PORT_STUB
    tusb_init();
#endif
}

hal_out_mode_t hal_out_mode(void) { return HAL_OUT_HID_DUBEOLSIK; }

bool hal_out_connected(void)
{
#ifdef CUIME_PORT_STUB
    return false;
#else
    return tud_hid_ready();
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
        default: return 0;
    }
}

static void hid_send(uint8_t keycode, bool shift)
{
#ifndef CUIME_PORT_STUB
    uint8_t kc[6] = { keycode, 0, 0, 0, 0, 0 };
    tud_hid_keyboard_report(0, shift ? 0x02 : 0x00, kc);
    sleep_ms(2);
    tud_hid_keyboard_report(0, 0, NULL);
    sleep_ms(2);
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

void hal_out_unicode(uint32_t cp) { (void)cp; }

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

bool hal_nvs_set_u32(const char *key, uint32_t val) { (void)key; (void)val; return false; }
bool hal_nvs_get_u32(const char *key, uint32_t *val) { (void)key; (void)val; return false; }

void hal_ui_preedit(uint32_t cp) { (void)cp; }
void hal_ui_layout(cuime_layout_t l) { (void)l; }
void hal_ui_candidates(const uint32_t *l, uint8_t n, uint8_t s) { (void)l; (void)n; (void)s; }
void hal_ui_warn(const char *m) { (void)m; }

void hal_board_init(void)
{
    suppress_unused();
    hal_keys_init();
    hal_out_init();
}
