/* 커스텀 HID 전송 계층 — TinyUSB 연동
 *
 * 디바이스는 키 이벤트만 보낸다. 조합은 호스트 IME 가 한다.
 *
 * ── IME 접속/이탈 판정 ──
 *   접속: 호스트가 CMD_HELLO 를 보냄 -> RAW 모드 진입
 *   이탈: (a) CMD_BYE 수신 (정상 종료)
 *         (b) 하트비트 3초 끊김 (강제 종료·크래시)
 *
 *   (b) 가 필요한 이유: 프로세스가 kill 되면 BYE 를 못 보낸다. 그대로
 *   RAW 모드에 남으면 키를 눌러도 아무 일도 안 일어나는 먹통 키보드가
 *   된다. 호스트가 살아있음을 주기적으로 증명하게 한다.
 */
#include "tusb.h"
#include "../../hal/cuime_hal.h"
#include "../../hal/cuime_rawhid.h"

enum { ITF_BOOT_KBD = 0, ITF_RAW = 1 };

static bool     s_ime_attached;
static uint32_t s_last_ping_ms;

/* hal_millis() 는 HAL 이 제공 */
extern uint32_t hal_millis(void);

/* ── HAL 구현: IME 접속 여부 ── */
bool hal_out_ime_attached(void)
{
    if (!s_ime_attached) return false;
    /* 하트비트 만료 검사. 부호 없는 뺄셈이라 타이머 래핑에도 안전하다. */
    if ((uint32_t)(hal_millis() - s_last_ping_ms) > CUIME_RAW_HEARTBEAT_MS) {
        s_ime_attached = false;      /* 자동 폴백 */
    }
    return s_ime_attached;
}

/* ── HAL 구현: 원시 키 이벤트 전송 ── */
void hal_out_keyevent(cuime_key_t key, bool pressed)
{
    if (!tud_hid_n_ready(ITF_RAW)) return;

    uint8_t r[CUIME_RAW_REPORT_SIZE] = {0};
    uint32_t ms = hal_millis();
    r[0] = CUIME_RAW_PROTO_VER;
    r[1] = CUIME_RAW_EV_KEY;
    r[2] = (uint8_t)key;
    r[3] = pressed ? 1 : 0;
    /* 타임스탬프를 함께 보낸다. USB 지연이 흔들려도 호스트가 멀티탭
     * 간격을 정확히 계산할 수 있다 — 도착 시각으로 재면 오판이 생긴다. */
    r[4] = (uint8_t)(ms & 0xFF);
    r[5] = (uint8_t)((ms >> 8) & 0xFF);
    r[6] = (uint8_t)((ms >> 16) & 0xFF);
    r[7] = (uint8_t)((ms >> 24) & 0xFF);

    tud_hid_n_report(ITF_RAW, 0, r, sizeof r);
}

/* ── 호스트 -> 디바이스 수신 ── */
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    (void)report_id;
    if (instance != ITF_RAW) return;
    if (report_type != HID_REPORT_TYPE_OUTPUT && report_type != 0) return;
    if (bufsize < 2) return;
    if (buffer[0] != CUIME_RAW_PROTO_VER) return;   /* 버전 불일치 무시 */

    switch (buffer[1]) {
    case CUIME_RAW_CMD_HELLO: {
        s_ime_attached = true;
        s_last_ping_ms = hal_millis();
        /* 응답: 프로토콜 버전과 키 개수를 알려 호스트가 호환성을 확인 */
        uint8_t r[CUIME_RAW_REPORT_SIZE] = {0};
        r[0] = CUIME_RAW_PROTO_VER;
        r[1] = CUIME_RAW_EV_HELLO;
        r[2] = CUIME_RAW_PROTO_VER;
        r[3] = CUIME_KEY__COUNT;
        const char *name = "cuime-f411";
        for (int i = 0; name[i] && i < 16; i++) r[4 + i] = (uint8_t)name[i];
        if (tud_hid_n_ready(ITF_RAW)) tud_hid_n_report(ITF_RAW, 0, r, sizeof r);
        break;
    }
    case CUIME_RAW_CMD_BYE:
        s_ime_attached = false;
        break;

    case CUIME_RAW_CMD_PING: {
        s_last_ping_ms = hal_millis();
        uint8_t r[CUIME_RAW_REPORT_SIZE] = {0};
        r[0] = CUIME_RAW_PROTO_VER;
        r[1] = CUIME_RAW_EV_PONG;
        r[2] = buffer[2]; r[3] = buffer[3];      /* nonce 반향 */
        r[4] = buffer[4]; r[5] = buffer[5];
        if (tud_hid_n_ready(ITF_RAW)) tud_hid_n_report(ITF_RAW, 0, r, sizeof r);
        break;
    }
    case CUIME_RAW_CMD_LED:
        hal_ui_preedit(buffer[2] ? 1u : 0u);     /* 호스트가 LED 제어 */
        break;

    default:
        break;
    }
}

/* GET_REPORT — 쓰지 않지만 TinyUSB 가 심볼을 요구한다 */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance; (void)report_id; (void)report_type;
    (void)buffer; (void)reqlen;
    return 0;
}

/* USB 케이블이 빠지면 즉시 폴백 상태로 되돌린다 */
void tud_umount_cb(void)   { s_ime_attached = false; }
void tud_suspend_cb(bool r){ (void)r; s_ime_attached = false; }
