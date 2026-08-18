/* USB 복합장치 디스크립터 — 커스텀 HID + 부트 키보드
 *
 *   Interface 0 : 부트 키보드   → IME 가 없을 때 두벌식 폴백
 *   Interface 1 : 커스텀 HID    → IME 가 원시 키 이벤트를 받아감
 *
 * 두 인터페이스가 **동시에** 노출된다. 모드 전환에 USB 재열거가 없다.
 * 어느 쪽을 쓸지는 호스트가 정한다: IME 가 HELLO 를 보내면 디바이스가
 * 부트 키보드를 침묵시키고, 하트비트가 끊기면 자동 복귀한다.
 *
 * ── 엔드포인트 예산 (F411 은 양방향 4개, EP0 포함) ──
 *   EP0     제어
 *   EP1 IN  부트 키보드
 *   EP2 IN  커스텀 HID (디바이스 -> 호스트)
 *   EP2 OUT 커스텀 HID (호스트 -> 디바이스)
 *   => IN 3개. 여유 1개. (CDC 를 썼다면 4개 전부 소진)
 */
#include "tusb.h"
#include "../../hal/cuime_rawhid.h"

/* VID/PID
 * 0x1209 는 pid.codes — 오픈소스/취미 프로젝트용 공용 VID.
 * 실제 판매 제품이라면 USB-IF 에서 VID 를 받아야 한다. */
#define CUIME_VID 0x1209
#define CUIME_PID 0xCE01

/* ── 장치 디스크립터 ── */
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    /* 복합장치지만 CDC 가 없으므로 IAD 불필요.
     * HID 인터페이스는 각각 독립이라 클래스를 0 으로 둔다. */
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = CUIME_VID,
    .idProduct          = CUIME_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

/* ── HID 리포트 디스크립터 ── */

/* 부트 키보드 — 표준 형식. OS 가 드라이버 없이 인식한다. */
uint8_t const desc_hid_boot_kbd[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

/* 커스텀 HID — cuime_rawhid.h 의 정본 매크로를 쓴다.
 * Usage != 0, Collection(Application) 조건은 거기서 보장한다. */
uint8_t const desc_hid_raw[] = {
    CUIME_RAW_HID_REPORT_DESC()
};

enum { ITF_NUM_BOOT_KBD = 0, ITF_NUM_RAW, ITF_NUM_TOTAL };

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    return (instance == ITF_NUM_BOOT_KBD) ? desc_hid_boot_kbd : desc_hid_raw;
}

/* ── 구성 디스크립터 ── */
#define EPNUM_BOOT_KBD  0x81
#define EPNUM_RAW_IN    0x82
#define EPNUM_RAW_OUT   0x02

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN \
                                               + TUD_HID_INOUT_DESC_LEN)

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    /* 부트 키보드: interval 10ms 면 충분(사람 타이핑 속도) */
    TUD_HID_DESCRIPTOR(ITF_NUM_BOOT_KBD, 4, HID_ITF_PROTOCOL_KEYBOARD,
                       sizeof(desc_hid_boot_kbd), EPNUM_BOOT_KBD,
                       CFG_TUD_HID_EP_BUFSIZE, 10),

    /* 커스텀 HID: 입출력 양방향. interval 1ms — 키 이벤트 지연을 줄인다 */
    TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_RAW, 5, HID_ITF_PROTOCOL_NONE,
                             sizeof(desc_hid_raw),
                             EPNUM_RAW_OUT, EPNUM_RAW_IN,
                             CFG_TUD_HID_EP_BUFSIZE, 1)
};

/* 길이 계산이 어긋나면 호스트가 파싱을 중간에 멈춰 인터페이스가 사라진다.
 * 흔한 실수라 컴파일 타임에 잡는다. */
_Static_assert(sizeof(desc_configuration) == CONFIG_TOTAL_LEN,
               "구성 디스크립터 길이 불일치 — wTotalLength 를 확인하라");

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

/* ── 문자열 디스크립터 ── */
static char const *string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 },   /* 0: 언어 = 영어(0x0409) */
    "cuime",                        /* 1: 제조사 */
    "Cheonjiin Keyboard",           /* 2: 제품 */
    "CUIME-F411-0001",              /* 3: 일련번호 */
    "Cheonjiin Boot Keyboard",      /* 4: 부트 키보드 인터페이스 */
    "Cheonjiin Raw HID",            /* 5: 커스텀 HID 인터페이스 */
};

static uint16_t _desc_str[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    uint8_t chr_count;

    if (index == 0) {
        _desc_str[1] = 0x0409;
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))
            return NULL;
        const char *str = string_desc_arr[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) _desc_str[1 + i] = str[i];
    }
    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
