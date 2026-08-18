/* 호스트 테스트용 가짜 레지스터
 *
 * CUIME_F411_HOSTTEST 정의 시 stm32f411_regs.h 대신 이 헤더가 쓰인다.
 * MMIO 주소를 일반 변수로 바꿔, F411 HAL 을 x86 에서 그대로 실행한다.
 * 하드웨어 접근만 가짜가 되고 매핑·디바운스·인코딩 로직은 원본 그대로다.
 *
 * 설계 메모: C 에는 대입 연산자 오버로드가 없으므로 "레지스터 쓰기 가로채기"를
 * 매크로로 흉내낼 수 없다. 대신
 *   - 행 구동   : BSRR 에 마지막으로 쓰인 값을 IDR 읽기 함수가 해석한다
 *                 (스캔은 BSRR 쓰기 직후 IDR 을 읽으므로 이걸로 충분하다)
 *   - UART 송신 : uart_putc 안에 호스트 전용 훅을 둔다
 * 로 처리한다. 검증 대상인 매핑/디바운스/UTF-8 로직은 손대지 않는다.
 */
#ifndef STM32F411_HOSTTEST_H
#define STM32F411_HOSTTEST_H

#include <stdint.h>
#include <stdbool.h>

#define GPIO_MODE_INPUT   0u
#define GPIO_MODE_OUTPUT  1u
#define GPIO_MODE_AF      2u
#define GPIO_MODE_ANALOG  3u
#define GPIO_PULL_NONE    0u
#define GPIO_PULL_UP      1u
#define GPIO_PULL_DOWN    2u

#define USART_SR_TXE   (1u << 7)
#define USART_SR_TC    (1u << 6)
#define USART_CR1_TE   (1u << 3)
#define USART_CR1_UE   (1u << 13)

#define RCC_AHB1ENR_GPIOAEN  (1u << 0)
#define RCC_AHB1ENR_GPIOBEN  (1u << 1)
#define RCC_AHB1ENR_GPIOCEN  (1u << 2)
#define RCC_APB1ENR_USART2EN (1u << 17)

/* ── 가짜 레지스터 (전부 평범한 변수) ── */
extern uint32_t fk_gpioa_moder, fk_gpioa_pupdr, fk_gpioa_afrl, fk_gpioa_bsrr;
extern uint32_t fk_gpiob_moder, fk_gpiob_bsrr;
extern uint32_t fk_gpioc_moder, fk_gpioc_bsrr;
extern uint32_t fk_rcc_ahb1enr, fk_rcc_apb1enr;
extern uint32_t fk_usart_brr, fk_usart_cr1, fk_usart_dr;

/* 테스트가 설정하는 "지금 눌린 키" (-1 = 없음) */
extern int fk_press_row, fk_press_col;

/* GPIOB_BSRR 최종값으로부터 구동 중인 행을 판정해 열 상태를 만든다 */
uint32_t fk_gpioa_idr_read(void);

/* UART 캡처 */
void        fk_uart_push(char c);
void        fake_uart_reset(void);
const char *fake_uart_hex(void);
/* LED(PC13): 마지막 BSRR 쓰기로 판정. true = 점등(Low) */
bool        fake_led_on(void);
void        fake_ime_attach(bool on);
void        fake_raw_reset(void);
const char *fake_raw_hex(void);
#ifdef CUIME_F411_USB_HID
void        fake_hid_reset(void);
const char *fake_hid_hex(void);
#endif

#define GPIOA_MODER  fk_gpioa_moder
#define GPIOA_PUPDR  fk_gpioa_pupdr
#define GPIOA_AFRL   fk_gpioa_afrl
#define GPIOA_BSRR   fk_gpioa_bsrr
#define GPIOA_IDR    (fk_gpioa_idr_read())

#define GPIOB_MODER  fk_gpiob_moder
#define GPIOB_BSRR   fk_gpiob_bsrr
#define GPIOB_IDR    (0xFFFFFFFFu)

#define GPIOC_MODER  fk_gpioc_moder
#define GPIOC_BSRR   fk_gpioc_bsrr

#define RCC_AHB1ENR  fk_rcc_ahb1enr
#define RCC_APB1ENR  fk_rcc_apb1enr

#define USART2_BRR   fk_usart_brr
#define USART2_CR1   fk_usart_cr1
#define USART2_DR    fk_usart_dr
#define USART2_SR    (USART_SR_TXE | USART_SR_TC)   /* 항상 송신 가능 */

#endif /* STM32F411_HOSTTEST_H */
