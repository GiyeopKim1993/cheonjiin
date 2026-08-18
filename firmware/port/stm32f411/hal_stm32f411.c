/* HAL 포트: STM32F411 (Black Pill / Nucleo-F411RE) — 데모 보드
 *
 * 대상: STM32F411CEU6 "Black Pill" (WeAct) 또는 Nucleo-F411RE
 *   - Cortex-M4F @ 100MHz (HSI 16MHz → PLL)
 *   - Flash 512KB / SRAM 128KB
 *   - USB OTG FS (PA11/PA12) — USB HID 키보드
 *
 * ⚠️ CubeMX/HAL 라이브러리를 쓰지 않는다. 레지스터를 직접 다뤄
 *    외부 SDK 없이 이 파일 + 스타트업 + 링커스크립트만으로 빌드된다.
 *    (CubeMX 기반 일반 STM32 포트는 ../stm32/hal_stm32.c 참조)
 *
 * 배선 — 4×4 매트릭스:
 *   행(출력, Push-Pull) : PB0 PB1 PB2 PB3
 *   열(입력, Pull-up)   : PA0 PA1 PA2 PA3
 *   상태 LED            : PC13 (Black Pill 내장, active-low)
 *
 * 키 배치는 docs/00-확정스펙.md §2 를 따른다.
 */
#include "../../hal/cuime_hal.h"
#ifdef CUIME_F411_HOSTTEST
#  include "stm32f411_hosttest.h"    /* x86 검증용 가짜 레지스터 */
#else
#  include "stm32f411_regs.h"
#endif

/* ─────────── 키 매트릭스 ─────────── */

#define ROW_COUNT 4
#define COL_COUNT 4

static const uint8_t ROW_PIN[ROW_COUNT] = { 0, 1, 2, 3 };   /* PB0..PB3 */
/* 열은 PA4..PA7 을 쓴다. PA0..PA3 을 쓰면 열2 가 USART2_TX(PA2) 와 충돌해
 * UART 출력 모드에서 키가 안 먹는다. 기존 stm32 포트와도 같은 배치다. */
static const uint8_t COL_PIN[COL_COUNT] = { 4, 5, 6, 7 };   /* PA4..PA7 */

/* (행,열) -> 논리 키 (docs/00 §2 확정 배치)
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

/* 디바운스: 연속 2회 동일해야 확정 (5ms 주기 → 10ms) */
#define DEBOUNCE_SAMPLES 2
static hal_keymask_t s_stable, s_last_raw;
static uint8_t s_stable_cnt;

/* ─────────── SysTick 밀리초 ─────────── */

static volatile uint32_t s_millis;

void SysTick_Handler(void)   /* 스타트업의 벡터 테이블에서 참조 */
{
    s_millis++;
}

uint32_t hal_millis(void) { return s_millis; }

void hal_delay_ms(uint32_t ms)
{
#ifdef CUIME_F411_HOSTTEST
    s_millis += ms;              /* 호스트: 시간만 진행 */
#else
    uint32_t t0 = s_millis;
    /* WFI 로 대기 — SysTick 인터럽트가 매 ms 깨운다 */
    while ((uint32_t)(s_millis - t0) < ms) {
        __asm volatile ("wfi");
    }
#endif
}

/* ─────────── 클럭: HSI 16MHz → PLL 100MHz ─────────── */

static void clock_init(void)
{
#ifdef CUIME_F411_HOSTTEST
    return;            /* 호스트: PLL 락 대기 루프를 돌 수 없다 */
#else
    /* 플래시 대기상태: 100MHz @ 3.3V → 3 wait states + 캐시/프리페치 */
    FLASH_ACR = FLASH_ACR_LATENCY_3WS | FLASH_ACR_PRFTEN
              | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /* HSI(16MHz) 안정화 대기 */
    RCC_CR |= RCC_CR_HSION;
    while (!(RCC_CR & RCC_CR_HSIRDY)) { }

    /* PLL: VCO = HSI/M*N = 16/8*100 = 200MHz
     *      SYSCLK = VCO/P = 200/2 = 100MHz
     *      USB    = VCO/Q = 200/4 = 50MHz  ← 48MHz 아님, 주의(아래 설명)
     *
     * USB FS 는 정확히 48MHz 를 요구한다. HSI(±1%) 로는 USB 규격을 못 맞춘다.
     * 데모에서는 HSE 25MHz(Black Pill 기본 크리스털) 사용을 권장한다:
     *      M=25, N=192, P=2, Q=4 → SYSCLK 96MHz, USB 48MHz ✅
     * CUIME_F411_USE_HSE 를 정의하면 그 설정을 쓴다.
     */
#ifdef CUIME_F411_USE_HSE
    RCC_CR |= RCC_CR_HSEON;
    while (!(RCC_CR & RCC_CR_HSERDY)) { }
    /* M=25 N=192 P=2(00) Q=4, HSE 소스 */
    RCC_PLLCFGR = (25u << 0) | (192u << 6) | (0u << 16) | (4u << 24)
                | RCC_PLLCFGR_PLLSRC_HSE;
#else
    /* M=8 N=100 P=2(00) Q=4, HSI 소스 — USB 는 비활성(UART 출력용) */
    RCC_PLLCFGR = (8u << 0) | (100u << 6) | (0u << 16) | (4u << 24);
#endif

    RCC_CR |= RCC_CR_PLLON;
    while (!(RCC_CR & RCC_CR_PLLRDY)) { }

    /* AHB=/1, APB1=/2(최대 50MHz), APB2=/1 */
    RCC_CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1;

    /* SYSCLK 을 PLL 로 전환 */
    RCC_CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC_CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL) { }
#endif
}

static void systick_init(uint32_t hclk_hz)
{
#ifdef CUIME_F411_HOSTTEST
    (void)hclk_hz; return;
#else
    SYST_RVR = (hclk_hz / 1000u) - 1u;   /* 1ms */
    SYST_CVR = 0;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;
#endif
}

/* ─────────── GPIO ─────────── */

static void gpio_mode(volatile uint32_t *moder, uint8_t pin, uint32_t mode)
{
    *moder = (*moder & ~(3u << (pin * 2))) | (mode << (pin * 2));
}

void hal_keys_init(void)
{
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

    /* 행: 출력, 초기 High */
    for (int r = 0; r < ROW_COUNT; r++) {
        gpio_mode(&GPIOB_MODER, ROW_PIN[r], GPIO_MODE_OUTPUT);
        GPIOB_BSRR = (1u << ROW_PIN[r]);
    }
    /* 열: 입력 + 풀업 */
    for (int c = 0; c < COL_COUNT; c++) {
        gpio_mode(&GPIOA_MODER, COL_PIN[c], GPIO_MODE_INPUT);
        GPIOA_PUPDR = (GPIOA_PUPDR & ~(3u << (COL_PIN[c] * 2)))
                    | (GPIO_PULL_UP << (COL_PIN[c] * 2));
    }
    /* LED PC13 출력 (active-low → 초기 소등) */
    gpio_mode(&GPIOC_MODER, 13, GPIO_MODE_OUTPUT);
    GPIOC_BSRR = (1u << 13);

    s_stable = 0; s_last_raw = 0; s_stable_cnt = 0;
}

static void short_delay(void)
{
#ifndef CUIME_F411_HOSTTEST
    /* 행 구동 후 신호 안정화 (~1us @100MHz) */
    for (volatile int i = 0; i < 40; i++) { }
#endif
}

static hal_keymask_t scan_raw(void)
{
    hal_keymask_t m = 0;
    for (int r = 0; r < ROW_COUNT; r++) {
        GPIOB_BSRR = (1u << (ROW_PIN[r] + 16));      /* 해당 행만 Low */
        short_delay();
        uint32_t idr = GPIOA_IDR;
        for (int c = 0; c < COL_COUNT; c++) {
            if (!(idr & (1u << COL_PIN[c])))          /* 눌리면 Low */
                m |= (hal_keymask_t)1u << KEYMAP[r][c];
        }
        GPIOB_BSRR = (1u << ROW_PIN[r]);              /* 복귀 High */
    }
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

/* ─────────── 출력 ───────────
 *
 * 데모 기본값은 UART2(PA2 TX, 115200) 로 UTF-8 을 그대로 내보낸다.
 * 보드만 있으면 USB 스택 없이도 동작을 눈으로 확인할 수 있다.
 *
 * CUIME_F411_USB_HID 를 정의하면 USB HID 두벌식 모드로 바뀐다.
 * (TinyUSB 등 USB 스택을 별도로 링크해야 하며, 그 훅은 아래 weak 함수)
 */

#ifdef CUIME_F411_USB_HID
/* USB 스택이 제공해야 하는 훅. 미구현이면 링크 시 weak 기본값 사용 */
#ifdef CUIME_F411_HOSTTEST
/* 호스트 테스트: 실제 USB 대신 캡처 버퍼로 */
void cuime_usb_hid_send(uint8_t modifier, uint8_t keycode);
bool cuime_usb_hid_ready(void) { return true; }
#else
__attribute__((weak)) void cuime_usb_hid_send(uint8_t modifier, uint8_t keycode)
{ (void)modifier; (void)keycode; }
__attribute__((weak)) bool cuime_usb_hid_ready(void) { return false; }
#endif
#endif

#ifndef CUIME_F411_USB_HID   /* HID 모드에서는 UART 경로를 쓰지 않는다 */
static void uart_init(void)
{
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC_APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PA2 = USART2_TX (AF7) */
    gpio_mode(&GPIOA_MODER, 2, GPIO_MODE_AF);
    GPIOA_AFRL = (GPIOA_AFRL & ~(0xFu << (2 * 4))) | (7u << (2 * 4));

    /* APB1 = 50MHz, 115200 → BRR = 50e6/115200 ≈ 434 */
    USART2_BRR = 434;
    USART2_CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void uart_putc(char c)
{
    while (!(USART2_SR & USART_SR_TXE)) { }
    USART2_DR = (uint32_t)(uint8_t)c;
#ifdef CUIME_F411_HOSTTEST
    fk_uart_push(c);            /* 호스트: 송신 바이트 캡처 */
#endif
}
#endif /* !CUIME_F411_USB_HID */

void hal_out_init(void)
{
#ifndef CUIME_F411_USB_HID
    uart_init();
#endif
    /* HID 모드의 USB 초기화는 USB 스택(TinyUSB 등) 담당 */
}

hal_out_mode_t hal_out_mode(void)
{
#if defined(CUIME_F411_RAWHID) || defined(CUIME_F411_HOSTTEST)
    /* 커스텀 HID 빌드: 호스트 IME 가 붙어 있으면 조합을 넘긴다.
     * IME 가 없으면 두벌식 폴백으로 동작해야 하므로 여기서 분기한다. */
    if (hal_out_ime_attached()) return HAL_OUT_RAW_KEYEVENT;
#endif
#ifdef CUIME_F411_USB_HID
    return HAL_OUT_HID_DUBEOLSIK;
#else
    return HAL_OUT_UNICODE_TEXT;   /* UART 데모 */
#endif
}

bool hal_out_connected(void)
{
#ifdef CUIME_F411_USB_HID
    return cuime_usb_hid_ready();
#else
    return true;                    /* UART 는 항상 연결로 간주 */
#endif
}

/* USB HID 키코드 (HID Usage Table 1.12) */
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

void hal_out_ascii(char c, bool shift)
{
#ifdef CUIME_F411_USB_HID
    bool need = false;
    uint8_t kc = ascii_to_hid(c, &need);
    if (kc) cuime_usb_hid_send((shift || need) ? 0x02 : 0x00, kc);
#else
    (void)shift;
    (void)ascii_to_hid;      /* UART 모드에서는 미사용 */
    uart_putc(c);
#endif
}

void hal_out_unicode(uint32_t cp)
{
#ifdef CUIME_F411_USB_HID
    (void)cp;                /* HID 는 유니코드 전송 불가 */
#else
    /* UTF-8 인코딩해 UART 로 */
    if (cp < 0x80) {
        uart_putc((char)cp);
    } else if (cp < 0x800) {
        uart_putc((char)(0xC0 | (cp >> 6)));
        uart_putc((char)(0x80 | (cp & 0x3F)));
    } else {
        uart_putc((char)(0xE0 | (cp >> 12)));
        uart_putc((char)(0x80 | ((cp >> 6) & 0x3F)));
        uart_putc((char)(0x80 | (cp & 0x3F)));
    }
#endif
}

void hal_out_special(hal_keycode_t k)
{
#ifdef CUIME_F411_USB_HID
    static const uint8_t KC[] = { 0x2A, 0x28, 0x2C, 0x50, 0x4F, 0x8A };
    if ((unsigned)k < sizeof KC) cuime_usb_hid_send(0x00, KC[k]);
#else
    switch (k) {
        case HAL_KEYCODE_BACKSPACE: uart_putc('\b'); break;
        case HAL_KEYCODE_ENTER:     uart_putc('\r'); uart_putc('\n'); break;
        case HAL_KEYCODE_SPACE:     uart_putc(' ');  break;
        case HAL_KEYCODE_LEFT:      break;   /* 터미널 커서 이동은 생략 */
        case HAL_KEYCODE_RIGHT:     break;
        case HAL_KEYCODE_HANJA:     break;
    }
#endif
}

/* ─────────── 설정 저장 ───────────
 * 내장 플래시 EEPROM 에뮬레이션은 데모 범위 밖. HAL 계약상 false 반환 허용.
 */
bool hal_nvs_set_u32(const char *key, uint32_t val) { (void)key; (void)val; return false; }
bool hal_nvs_get_u32(const char *key, uint32_t *val) { (void)key; (void)val; return false; }

/* ─────────── 표시 ───────────
 * 디스플레이가 없으므로 LED 로 최소 피드백만.
 *   조합 중  : LED 점등
 *   확정/유휴: LED 소등
 */
void hal_ui_preedit(uint32_t cp)
{
    if (cp) GPIOC_BSRR = (1u << (13 + 16));   /* PC13 Low = 점등 */
    else    GPIOC_BSRR = (1u << 13);          /* High = 소등 */
}

void hal_ui_layout(cuime_layout_t l)
{
    /* 레이아웃 전환을 짧은 깜빡임으로 알림 */
    (void)l;
    for (int i = 0; i < 2; i++) {
        GPIOC_BSRR = (1u << (13 + 16));
#ifndef CUIME_F411_HOSTTEST
        for (volatile int d = 0; d < 200000; d++) { }
#endif
        GPIOC_BSRR = (1u << 13);
#ifndef CUIME_F411_HOSTTEST
        for (volatile int d = 0; d < 200000; d++) { }
#endif
    }
}

void hal_ui_candidates(const uint32_t *l, uint8_t n, uint8_t s)
{ (void)l; (void)n; (void)s; }

void hal_ui_warn(const char *m) { (void)m; }

/* ─────────── 보드 초기화 ─────────── */

void hal_board_init(void)
{
    clock_init();
#ifdef CUIME_F411_USE_HSE
    systick_init(96000000u);
#else
    systick_init(100000000u);
#endif
    hal_keys_init();
    hal_out_init();
}

/* ── 원시 키 이벤트 (RAWHID 빌드가 아닐 때의 기본 구현) ──
 * RAWHID=1 이면 usb_rawhid.c 가 진짜 구현을 제공한다.
 * 그 외 빌드(UART 데모·두벌식 HID)에서는 전송 경로가 없으므로
 * ime_attached 를 false 로 두어 조합 경로로만 동작하게 한다.
 */
#if !defined(CUIME_F411_RAWHID) && !defined(CUIME_F411_HOSTTEST)
void hal_out_keyevent(cuime_key_t key, bool pressed)
{
    (void)key; (void)pressed;
}
bool hal_out_ime_attached(void) { return false; }
#endif
