/* 가짜 레지스터 구현 — 호스트 테스트 전용 */
#include "../port/stm32f411/stm32f411_hosttest.h"
#include <stdio.h>
#include <string.h>

uint32_t fk_gpioa_moder, fk_gpioa_pupdr, fk_gpioa_afrl, fk_gpioa_bsrr;
uint32_t fk_gpiob_moder, fk_gpiob_bsrr;
uint32_t fk_gpioc_moder, fk_gpioc_bsrr;
uint32_t fk_rcc_ahb1enr, fk_rcc_apb1enr;
uint32_t fk_usart_brr, fk_usart_cr1, fk_usart_dr;

int fk_press_row = -1, fk_press_col = -1;

/* 열 입력 계산.
 * 스캔 루틴은 "행 r 을 Low 로 → IDR 읽기 → 행 r 을 High 복귀" 순서다.
 * 따라서 IDR 을 읽는 시점의 GPIOB_BSRR 최종값이 곧 구동 중인 행이다.
 * BSRR 하위 16비트 = Set(High), 상위 16비트 = Reset(Low).
 * 풀업이므로 기본 High(1), 눌린 교차점만 Low(0).
 */
/* HAL 의 COL_PIN[] 복제본. 바뀌면 test_f411.c 가 대조해 잡는다. */
static const uint8_t COL_PIN_MIRROR[4] = { 4, 5, 6, 7 };   /* PA4..PA7 */
const uint8_t *fk_col_pin_mirror(void) { return COL_PIN_MIRROR; }

uint32_t fk_gpioa_idr_read(void)
{
    uint32_t idr = 0xFFFFFFFFu;              /* 전부 High = 미압하 */
    uint32_t reset_bits = (fk_gpiob_bsrr >> 16) & 0xFFFFu;

    if (fk_press_row >= 0 && fk_press_col >= 0) {
        /* 눌린 행이 지금 Low 로 구동 중일 때만 열이 Low 로 끌린다.
         * fk_press_col 은 **열 인덱스(0~3)** 이고 실제 IDR 비트는 핀 번호다.
         * HAL 의 COL_PIN[] 과 같은 매핑을 여기서도 적용해야 한다. */
        if ((reset_bits & (1u << fk_press_row)) && fk_press_col < 4)
            idr &= ~(1u << COL_PIN_MIRROR[fk_press_col]);
    }
    return idr;
}

/* ── UART 캡처 ── */
static unsigned char cap[256];
static int cap_n;
static char hexbuf[600];

void fk_uart_push(char c)
{
    if (cap_n < (int)sizeof cap) cap[cap_n++] = (unsigned char)c;
}

void fake_uart_reset(void) { cap_n = 0; hexbuf[0] = 0; }

const char *fake_uart_hex(void)
{
    int p = 0;
    for (int i = 0; i < cap_n && p + 3 < (int)sizeof hexbuf; i++)
        p += snprintf(hexbuf + p, sizeof hexbuf - p, "%02X", cap[i]);
    hexbuf[p] = 0;
    return hexbuf;
}

bool fake_led_on(void)
{
    /* PC13 을 Low(Reset)로 쓴 것이 마지막이면 점등 */
    return (fk_gpioc_bsrr & (1u << (13 + 16))) != 0;
}

/* ── HID 캡처 (CUIME_F411_USB_HID 빌드에서만 링크됨) ── */
#ifdef CUIME_F411_USB_HID
static unsigned char hid_cap[128];
static int hid_n;
static char hid_hex[400];

void cuime_usb_hid_send(uint8_t modifier, uint8_t keycode)
{
    (void)modifier;
    if (hid_n < (int)sizeof hid_cap) hid_cap[hid_n++] = keycode;
}
void fake_hid_reset(void) { hid_n = 0; hid_hex[0] = 0; }
const char *fake_hid_hex(void)
{
    int p = 0;
    for (int i = 0; i < hid_n && p + 3 < (int)sizeof hid_hex; i++)
        p += snprintf(hid_hex + p, sizeof hid_hex - p, "%02X", hid_cap[i]);
    hid_hex[p] = 0;
    return hid_hex;
}
#endif
