/* STM32F411 레지스터 정의 — 이 데모에 필요한 최소분만
 *
 * 출처: RM0383 (STM32F411xC/E Reference Manual), PM0214 (Cortex-M4 프로그래밍 매뉴얼)
 * CubeMX/CMSIS 없이 빌드하기 위한 것이므로, 쓰는 레지스터만 정의한다.
 *
 * ⚠️ 주소·비트 위치는 데이터시트 값이다. 수정 시 반드시 RM0383 대조할 것.
 */
#ifndef STM32F411_REGS_H
#define STM32F411_REGS_H

#include <stdint.h>

#define MMIO32(addr) (*(volatile uint32_t *)(addr))

/* ─── 베이스 주소 (RM0383 §2.3 메모리 맵) ─── */
#define PERIPH_BASE   0x40000000u
#define APB1_BASE     (PERIPH_BASE + 0x00000000u)
#define APB2_BASE     (PERIPH_BASE + 0x00010000u)
#define AHB1_BASE     (PERIPH_BASE + 0x00020000u)

#define GPIOA_BASE    (AHB1_BASE + 0x0000u)
#define GPIOB_BASE    (AHB1_BASE + 0x0400u)
#define GPIOC_BASE    (AHB1_BASE + 0x0800u)
#define RCC_BASE      (AHB1_BASE + 0x3800u)
#define FLASH_R_BASE  (AHB1_BASE + 0x3C00u)
#define USART2_BASE   (APB1_BASE + 0x4400u)

/* ─── GPIO (RM0383 §8.4) ─── */
#define GPIOA_MODER   MMIO32(GPIOA_BASE + 0x00)
#define GPIOA_PUPDR   MMIO32(GPIOA_BASE + 0x0C)
#define GPIOA_IDR     MMIO32(GPIOA_BASE + 0x10)
#define GPIOA_BSRR    MMIO32(GPIOA_BASE + 0x18)
#define GPIOA_AFRL    MMIO32(GPIOA_BASE + 0x20)

#define GPIOB_MODER   MMIO32(GPIOB_BASE + 0x00)
#define GPIOB_IDR     MMIO32(GPIOB_BASE + 0x10)
#define GPIOB_BSRR    MMIO32(GPIOB_BASE + 0x18)

#define GPIOC_MODER   MMIO32(GPIOC_BASE + 0x00)
#define GPIOC_BSRR    MMIO32(GPIOC_BASE + 0x18)

#define GPIO_MODE_INPUT   0u
#define GPIO_MODE_OUTPUT  1u
#define GPIO_MODE_AF      2u
#define GPIO_MODE_ANALOG  3u

#define GPIO_PULL_NONE    0u
#define GPIO_PULL_UP      1u
#define GPIO_PULL_DOWN    2u

/* ─── RCC (RM0383 §6.3) ─── */
#define RCC_CR        MMIO32(RCC_BASE + 0x00)
#define RCC_PLLCFGR   MMIO32(RCC_BASE + 0x04)
#define RCC_CFGR      MMIO32(RCC_BASE + 0x08)
#define RCC_AHB1ENR   MMIO32(RCC_BASE + 0x30)
#define RCC_APB1ENR   MMIO32(RCC_BASE + 0x40)
#define RCC_APB2ENR   MMIO32(RCC_BASE + 0x44)

#define RCC_CR_HSION       (1u << 0)
#define RCC_CR_HSIRDY      (1u << 1)
#define RCC_CR_HSEON       (1u << 16)
#define RCC_CR_HSERDY      (1u << 17)
#define RCC_CR_PLLON       (1u << 24)
#define RCC_CR_PLLRDY      (1u << 25)

#define RCC_PLLCFGR_PLLSRC_HSE (1u << 22)

/* CFGR: SW[1:0] SWS[3:2] HPRE[7:4] PPRE1[12:10] PPRE2[15:13] */
#define RCC_CFGR_SW_PLL     (2u << 0)
#define RCC_CFGR_SWS_MASK   (3u << 2)
#define RCC_CFGR_SWS_PLL    (2u << 2)
#define RCC_CFGR_HPRE_DIV1  (0u << 4)
#define RCC_CFGR_PPRE1_DIV2 (4u << 10)
#define RCC_CFGR_PPRE2_DIV1 (0u << 13)

#define RCC_AHB1ENR_GPIOAEN (1u << 0)
#define RCC_AHB1ENR_GPIOBEN (1u << 1)
#define RCC_AHB1ENR_GPIOCEN (1u << 2)
#define RCC_APB1ENR_USART2EN (1u << 17)

/* ─── FLASH (RM0383 §3.4) ─── */
#define FLASH_ACR     MMIO32(FLASH_R_BASE + 0x00)
#define FLASH_ACR_LATENCY_3WS (3u << 0)
#define FLASH_ACR_PRFTEN      (1u << 8)
#define FLASH_ACR_ICEN        (1u << 9)
#define FLASH_ACR_DCEN        (1u << 10)

/* ─── USART (RM0383 §19.6) ─── */
#define USART2_SR     MMIO32(USART2_BASE + 0x00)
#define USART2_DR     MMIO32(USART2_BASE + 0x04)
#define USART2_BRR    MMIO32(USART2_BASE + 0x08)
#define USART2_CR1    MMIO32(USART2_BASE + 0x0C)

#define USART_SR_TXE   (1u << 7)
#define USART_SR_TC    (1u << 6)
#define USART_CR1_TE   (1u << 3)
#define USART_CR1_UE   (1u << 13)

/* ─── SysTick (PM0214 §4.5) ─── */
#define SYST_BASE     0xE000E010u
#define SYST_CSR      MMIO32(SYST_BASE + 0x00)
#define SYST_RVR      MMIO32(SYST_BASE + 0x04)
#define SYST_CVR      MMIO32(SYST_BASE + 0x08)

#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_TICKINT   (1u << 1)
#define SYST_CSR_CLKSOURCE (1u << 2)   /* 프로세서 클럭 사용 */

#endif /* STM32F411_REGS_H */
