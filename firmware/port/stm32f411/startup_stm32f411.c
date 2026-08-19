/* STM32F411 스타트업 — C 로 작성 (어셈블리 불필요)
 *
 * 리셋 시:
 *   1. .data 를 플래시에서 SRAM 으로 복사
 *   2. .bss 를 0 으로 초기화
 *   3. main() 호출
 *
 * 벡터 테이블은 링커스크립트가 0x08000000 최상단에 배치한다.
 */
#include <stdint.h>

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;

int main(void);
void SysTick_Handler(void);          /* hal_stm32f411.c 제공 */

/* 미정의 인터럽트는 여기로 — 디버깅 시 여기서 멈춘다 */
void Default_Handler(void)
{
    while (1) { __asm volatile ("wfi"); }
}

void Reset_Handler(void)
{
    /* .data 복사 (플래시 → SRAM) */
    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;

    /* .bss 클리어 */
    for (dst = &_sbss; dst < &_ebss; ) *dst++ = 0;

    /* FPU 활성화 (Cortex-M4F, CPACR CP10/CP11 full access) */
    *(volatile uint32_t *)0xE000ED88u |= (0xFu << 20);
    __asm volatile ("dsb");
    __asm volatile ("isb");

    main();
    while (1) { }        /* main 이 반환하면 정지 */
}

/* 약한 별칭 — 필요하면 각 포트에서 재정의 */
#define WEAK_ALIAS __attribute__((weak, alias("Default_Handler")))
void NMI_Handler(void)        WEAK_ALIAS;
void HardFault_Handler(void)  WEAK_ALIAS;
void MemManage_Handler(void)  WEAK_ALIAS;
void BusFault_Handler(void)   WEAK_ALIAS;
void UsageFault_Handler(void) WEAK_ALIAS;
void SVC_Handler(void)        WEAK_ALIAS;
void DebugMon_Handler(void)   WEAK_ALIAS;
void PendSV_Handler(void)     WEAK_ALIAS;

/* 벡터 테이블 — Cortex-M 코어 예외 16개까지만 (주변장치 IRQ 는 미사용) */
__attribute__((section(".isr_vector"), used))
void (* const g_vectors[])(void) = {
    (void (*)(void))&_estack,   /* 0: 초기 스택 포인터 */
    Reset_Handler,              /* 1: 리셋 */
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,                 /* 예약 */
    SVC_Handler,
    DebugMon_Handler,
    0,                          /* 예약 */
    PendSV_Handler,
    SysTick_Handler,            /* 15: 1ms 틱 */
};
