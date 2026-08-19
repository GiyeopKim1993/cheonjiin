/* CMSIS 대체 shim — TinyUSB dcd_dwc2 가 요구하는 최소분만
 *
 * 이 프로젝트는 CubeMX/CMSIS 없이 레지스터를 직접 제어한다. 그런데
 * TinyUSB 의 STM32 포트는 "stm32f4xx.h" (ST 제공 CMSIS 헤더)를 include 한다.
 * CMSIS 전체(수 MB)를 끌어오는 대신, 실제로 참조되는 심볼 7개만 정의한다.
 *
 * 참조되는 것 (dwc2_stm32.h 에서 grep 으로 확인):
 *   USB_OTG_FS_PERIPH_BASE, USB_OTG_FS_MAX_IN_ENDPOINTS,
 *   USB_OTG_FS_TOTAL_FIFO_SIZE, OTG_FS_IRQn,
 *   NVIC_EnableIRQ, NVIC_DisableIRQ, SystemCoreClock
 *
 * 값 출처: RM0383 (STM32F411) / STM32F411 데이터시트
 */
#ifndef CUIME_STM32F4XX_SHIM_H
#define CUIME_STM32F4XX_SHIM_H

#include <stdint.h>

/* ── USB OTG FS ──
 * F411 은 FS 만 있다(HS 없음). USB_OTG_HS_PERIPH_BASE 를 정의하지 않으면
 * TinyUSB 가 DWC2_EP_MAX 를 FS 값으로 잡는다. */
#define USB_OTG_FS_PERIPH_BASE        0x50000000UL

/* F411: 양방향 엔드포인트 4개 (EP0 포함) — 데이터시트 확인값.
 * 이 값이 실제보다 크면 TinyUSB 가 없는 EP 를 건드려 하드폴트가 난다. */
#define USB_OTG_FS_MAX_IN_ENDPOINTS   4U

/* 전용 패킷 버퍼 SRAM 1.25KB = 320 x 32bit 워드 */
#define USB_OTG_FS_TOTAL_FIFO_SIZE    1280U

/* ── NVIC ──
 * F411 벡터 테이블에서 OTG_FS 는 IRQ 67 (RM0383 표 38). */
typedef enum {
    OTG_FS_IRQn = 67
} IRQn_Type;

#define NVIC_BASE_ADDR 0xE000E100UL

static inline void NVIC_EnableIRQ(IRQn_Type irq)
{
    uint32_t n = (uint32_t)irq;
    /* ISER[n/32] 의 (n%32) 비트 */
    *(volatile uint32_t *)(NVIC_BASE_ADDR + 0x00 + 4UL * (n >> 5)) =
        (uint32_t)(1UL << (n & 0x1FUL));
}

static inline void NVIC_DisableIRQ(IRQn_Type irq)
{
    uint32_t n = (uint32_t)irq;
    /* ICER 은 NVIC_BASE + 0x80 */
    *(volatile uint32_t *)(NVIC_BASE_ADDR + 0x80 + 4UL * (n >> 5)) =
        (uint32_t)(1UL << (n & 0x1FUL));
    __asm volatile ("dsb");
    __asm volatile ("isb");
}

/* CMSIS 내장 함수 — 지연 루프에서 쓰인다 */
static inline void __NOP(void) { __asm volatile ("nop"); }

/* 클럭 초기화(hal_stm32f411.c) 가 설정한 값과 일치해야 한다.
 * USB 를 쓰려면 HSE 96MHz 구성이 필수다(HSI 로는 USB 48MHz 를 못 만든다). */
extern uint32_t SystemCoreClock;

#endif /* CUIME_STM32F4XX_SHIM_H */
