/* STM32F411CE 링커 스크립트
 *   Flash 512KB @ 0x08000000
 *   SRAM  128KB @ 0x20000000
 *
 * STM32F411CC(256KB) 를 쓰면 FLASH LENGTH 를 256K 로 바꾼다.
 */
ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* 스택 1KB — 이 펌웨어는 재귀가 없고 지역변수도 작다.
 * cuime_app_t 는 전역이라 스택을 쓰지 않는다. */
_stack_size = 1K;
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .isr_vector : {
    KEEP(*(.isr_vector))
  } > FLASH

  .text : {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.rodata)
    *(.rodata*)     /* 천지인 테이블 7KB 가 여기 — 플래시 상주 */
    . = ALIGN(4);
  } > FLASH

  /* 예외 처리 테이블 (사용 안 하지만 링커가 요구할 수 있음) */
  .ARM.extab : { *(.ARM.extab*) } > FLASH
  .ARM.exidx : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > FLASH

  _sidata = LOADADDR(.data);

  .data : {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  .bss : {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  /* 스택 여유 확인 — 부족하면 링크 에러 */
  ._user_heap_stack : {
    . = ALIGN(8);
    . = . + _stack_size;
    . = ALIGN(8);
  } > RAM

  /DISCARD/ : {
    *(.note.GNU-stack)
    *(.gnu_debuglink)
    *(.gnu.lto_*)
  }
}
