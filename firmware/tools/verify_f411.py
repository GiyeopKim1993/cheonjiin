#!/usr/bin/env python3
"""STM32F411 펌웨어 ELF 검증

보드에 굽기 전에, 컴파일은 됐지만 **부팅하지 않는** 흔한 실패를 잡는다.
실기기 없이 확인할 수 있는 것들이다.

검사 항목:
  1. 벡터 테이블이 0x08000000 에 있는가
  2. 초기 SP 가 SRAM 범위(0x20000000~0x20020000)인가
  3. Reset 벡터의 thumb 비트(bit0)가 1인가  ← 0이면 즉시 HardFault
  4. SysTick 벡터(#15)가 Default_Handler 가 아닌가 ← 아니면 시간이 안 흐름
  5. FLASH/RAM 사용량이 용량 안에 들어가는가
  6. 핵심 심볼이 gc-sections 로 잘려나가지 않았는가

사용법: verify_f411.py <firmware.elf>
"""
import struct, sys

FLASH_BASE, FLASH_SIZE = 0x08000000, 512 * 1024
RAM_BASE,   RAM_SIZE   = 0x20000000, 128 * 1024

# 이 데모가 **실제로 호출하는** 심볼만 넣는다.
# cuime_flush 처럼 호출되지 않는 API 는 gc-sections 가 지우는 게 정상이며,
# 그걸 필수로 넣으면 검증기가 거짓 실패를 낸다(실제로 겪음).
REQUIRED_SYMBOLS = [
    'main', 'Reset_Handler', 'SysTick_Handler',
    'cuime_app_poll', 'cuime_key_down',
    'hal_keys_scan', 'hal_millis', 'hal_board_init',
]


def sections(d):
    e_shoff     = struct.unpack_from('<I', d, 0x20)[0]
    e_shentsize = struct.unpack_from('<H', d, 0x2E)[0]
    e_shnum     = struct.unpack_from('<H', d, 0x30)[0]
    e_shstrndx  = struct.unpack_from('<H', d, 0x32)[0]

    def sh(i):
        return struct.unpack_from('<IIIIIIIIII', d, e_shoff + i * e_shentsize)

    stroff = sh(e_shstrndx)[4]

    def nm(off):
        return d[stroff + off:d.index(b'\0', stroff + off)].decode()

    out = []
    for i in range(e_shnum):
        name, typ, flags, addr, off, size, link, info, align, entsz = sh(i)
        out.append({'name': nm(name), 'type': typ, 'addr': addr,
                    'off': off, 'size': size, 'link': link})
    return out


def symbols(d, secs):
    symtab = next((s for s in secs if s['name'] == '.symtab'), None)
    strtab = next((s for s in secs if s['name'] == '.strtab'), None)
    if not symtab or not strtab:
        return {}
    to = strtab['off']

    def sname(o):
        return d[to + o:d.index(b'\0', to + o)].decode()

    out = {}
    for i in range(symtab['size'] // 16):
        nameoff, value, size, info, other, shndx = struct.unpack_from(
            '<IIIBBH', d, symtab['off'] + i * 16)
        s = sname(nameoff)
        if s:
            out[s] = (value, size)
    return out


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    d = open(sys.argv[1], 'rb').read()
    secs = secs_ = sections(d)
    syms = symbols(d, secs)
    fails = []

    def chk(cond, ok_msg, fail_msg):
        if cond:
            print(f"   ✅ {ok_msg}")
        else:
            print(f"   ❌ {fail_msg}")
            fails.append(fail_msg)

    # 1. 벡터 테이블 위치
    vec = next((s for s in secs if s['name'] == '.isr_vector'), None)
    chk(vec is not None and vec['addr'] == FLASH_BASE,
        f"벡터 테이블 @ 0x{FLASH_BASE:08x}",
        "벡터 테이블이 0x08000000 에 없다 — 부팅 불가")

    if vec:
        raw = d[vec['off']:vec['off'] + vec['size']]
        sp, reset = struct.unpack_from('<II', raw, 0)

        # 2. 초기 SP
        chk(RAM_BASE < sp <= RAM_BASE + RAM_SIZE,
            f"초기 SP 0x{sp:08x} (SRAM 범위)",
            f"초기 SP 0x{sp:08x} 가 SRAM 밖")

        # 3. thumb 비트
        chk(reset & 1,
            f"Reset 0x{reset:08x} thumb 비트 설정됨",
            f"Reset 0x{reset:08x} thumb 비트 0 — 즉시 HardFault")

        # 4. SysTick 벡터 (index 15)
        if len(raw) >= 64:
            systick = struct.unpack_from('<I', raw, 15 * 4)[0]
            dflt = syms.get('Default_Handler', (0, 0))[0]
            chk(systick != 0 and (dflt == 0 or systick != dflt),
                f"SysTick 벡터 0x{systick:08x} 연결됨",
                "SysTick 이 Default_Handler — hal_millis() 가 멈춘다")

    # 5. 메모리 사용량
    flash = sum(s['size'] for s in secs
                if FLASH_BASE <= s['addr'] < FLASH_BASE + FLASH_SIZE)
    ram = sum(s['size'] for s in secs
              if RAM_BASE <= s['addr'] < RAM_BASE + RAM_SIZE)
    chk(flash <= FLASH_SIZE,
        f"FLASH {flash:,} B / {FLASH_SIZE:,} B ({flash/FLASH_SIZE*100:.2f}%)",
        f"FLASH 초과: {flash:,} B")
    chk(ram <= RAM_SIZE,
        f"RAM   {ram:,} B / {RAM_SIZE:,} B ({ram/RAM_SIZE*100:.2f}%)",
        f"RAM 초과: {ram:,} B")

    # 6. 핵심 심볼
    missing = [s for s in REQUIRED_SYMBOLS if s not in syms]
    chk(not missing,
        f"핵심 심볼 {len(REQUIRED_SYMBOLS)}종 모두 존재",
        f"심볼 누락(gc-sections?): {missing}")

    if fails:
        print(f"\n❌ 검증 실패 {len(fails)}건")
        return 1
    print("\n✅ 펌웨어 검증 통과 — 플래싱 가능")
    return 0


if __name__ == '__main__':
    sys.exit(main())
