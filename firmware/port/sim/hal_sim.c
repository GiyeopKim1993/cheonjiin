/* HAL 포트: 호스트 시뮬레이터
 *
 * 실제 하드웨어 없이 펌웨어 전체(core+app+HAL)를 검증하기 위한 포트.
 * 키 입력은 프로그램이 주입하고, 출력은 버퍼에 쌓아 대조한다.
 *
 * 새 보드 포팅의 참고 구현이기도 하다 — 이 파일이 구현하는 함수 목록이
 * 곧 포팅에 필요한 전부다.
 */
#include "../../hal/cuime_hal.h"
#include <stdio.h>
#include <string.h>

/* ── 시뮬레이터 상태 ── */
static hal_keymask_t s_keys = 0;
static uint32_t      s_now  = 0;
static hal_out_mode_t s_mode = HAL_OUT_HID_DUBEOLSIK;

/* 출력 버퍼 (테스트가 읽는다) */
char sim_out[4096];
int  sim_out_len = 0;
uint32_t sim_preedit = 0;
cuime_layout_t sim_layout = CUIME_LAYOUT_HANGUL;
uint32_t sim_cands[16];
uint8_t  sim_cand_count = 0;
char sim_warn[128];

void sim_reset(void)
{
    s_keys = 0; s_now = 0;
    sim_out_len = 0; sim_out[0] = 0;
    sim_preedit = 0; sim_layout = CUIME_LAYOUT_HANGUL;
    sim_cand_count = 0; sim_warn[0] = 0;
}
void sim_set_time(uint32_t ms) { s_now = ms; }
void sim_advance(uint32_t ms)  { s_now += ms; }
void sim_press(int key)   { s_keys |=  ((hal_keymask_t)1u << key); }
void sim_release(int key) { s_keys &= ~((hal_keymask_t)1u << key); }
void sim_set_mode(hal_out_mode_t m) { s_mode = m; }

static void out_putc(char c)
{
    if (sim_out_len < (int)sizeof(sim_out) - 1) {
        sim_out[sim_out_len++] = c;
        sim_out[sim_out_len] = 0;
    }
}
static void out_utf8(uint32_t cp)
{
    if (cp < 0x80) out_putc((char)cp);
    else if (cp < 0x800) {
        out_putc((char)(0xC0 | (cp >> 6)));
        out_putc((char)(0x80 | (cp & 0x3F)));
    } else {
        out_putc((char)(0xE0 | (cp >> 12)));
        out_putc((char)(0x80 | ((cp >> 6) & 0x3F)));
        out_putc((char)(0x80 | (cp & 0x3F)));
    }
}

/* ── HAL 구현 ── */

void hal_keys_init(void) { s_keys = 0; }
hal_keymask_t hal_keys_scan(void) { return s_keys; }

uint32_t hal_millis(void) { return s_now; }
void hal_delay_ms(uint32_t ms) { s_now += ms; }

void hal_out_init(void) {}
hal_out_mode_t hal_out_mode(void) { return s_mode; }
bool hal_out_connected(void) { return true; }

void hal_out_ascii(char c, bool shift)
{
    /* 두벌식 HID: shift 는 대문자로 표기해 테스트가 구분할 수 있게 */
    if (shift && c >= 'a' && c <= 'z') out_putc((char)(c - 32));
    else out_putc(c);
}

void hal_out_unicode(uint32_t cp) { out_utf8(cp); }

void hal_out_special(hal_keycode_t k)
{
    switch (k) {
    case HAL_KEYCODE_BACKSPACE:
        if (sim_out_len > 0) {           /* 호스트 백스페이스 모사 */
            int i = sim_out_len - 1;
            while (i > 0 && ((unsigned char)sim_out[i] & 0xC0) == 0x80) i--;
            sim_out_len = i; sim_out[i] = 0;
        }
        break;
    case HAL_KEYCODE_ENTER: out_putc('\n'); break;
    case HAL_KEYCODE_SPACE: out_putc(' ');  break;
    case HAL_KEYCODE_LEFT:  break;
    case HAL_KEYCODE_RIGHT: break;
    case HAL_KEYCODE_HANJA: break;
    }
}

bool hal_nvs_set_u32(const char *key, uint32_t val) { (void)key; (void)val; return false; }
bool hal_nvs_get_u32(const char *key, uint32_t *val) { (void)key; (void)val; return false; }

void hal_ui_preedit(uint32_t cp) { sim_preedit = cp; }
void hal_ui_layout(cuime_layout_t l) { sim_layout = l; }
void hal_ui_candidates(const uint32_t *list, uint8_t count, uint8_t sel)
{
    (void)sel;
    sim_cand_count = count;
    for (uint8_t i = 0; i < count && i < 16; i++) sim_cands[i] = list[i];
}
void hal_ui_warn(const char *msg)
{
    if (msg) { strncpy(sim_warn, msg, sizeof(sim_warn) - 1); sim_warn[sizeof(sim_warn)-1] = 0; }
    else sim_warn[0] = 0;
}

void hal_board_init(void) { hal_keys_init(); hal_out_init(); }

/* ── 원시 키 이벤트 (HAL_OUT_RAW_KEYEVENT) ──
 * 이 포트는 RAW 전송 경로가 없다. hal_out_mode() 가 RAW 를 반환하지 않으므로
 * app 계층이 이 함수를 호출하지 않지만, 링크 심볼은 있어야 한다.
 * ime_attached 가 항상 false 이므로 조합 경로로만 동작한다.
 */
void hal_out_keyevent(cuime_key_t key, bool pressed)
{
    (void)key; (void)pressed;
}

bool hal_out_ime_attached(void) { return false; }
