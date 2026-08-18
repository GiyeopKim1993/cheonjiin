#!/usr/bin/env python3
"""ELF -> raw binary + Intel HEX (objcopy 없이)

zig 툴체인에는 objcopy 가 없어 직접 만든다.

⚠️ 반드시 **플래시 영역 세그먼트만** 추출할 것.
   RAM(0x20000000) 세그먼트를 함께 넣으면 플래시~RAM 사이 빈 공간까지
   0xFF 로 채워져 402MB 짜리 .bin 이 나온다(실제로 겪음).

사용법: elf2bin.py <input.elf> <output-prefix>
        -> output-prefix.bin, output-prefix.hex
"""
import struct, sys

FLASH_BASE = 0x08000000
FLASH_END  = 0x08100000          # 1MB 상한 (F411 최대 512KB + 여유)
PT_LOAD    = 1


def load_segments(data):
    if data[:4] != b'\x7fELF':
        raise SystemExit("❌ ELF 파일이 아님")
    e_phoff    = struct.unpack_from('<I', data, 0x1C)[0]
    e_phentsize= struct.unpack_from('<H', data, 0x2A)[0]
    e_phnum    = struct.unpack_from('<H', data, 0x2C)[0]
    segs = []
    for i in range(e_phnum):
        t, off, va, pa, fsz, msz, fl, al = struct.unpack_from(
            '<IIIIIIII', data, e_phoff + i * e_phentsize)
        if t != PT_LOAD or fsz == 0:
            continue
        # LMA(paddr) 기준. .data 는 VMA=RAM 이지만 LMA=플래시라 여기 포함된다.
        if not (FLASH_BASE <= pa < FLASH_END):
            continue
        segs.append((pa, off, fsz))
    if not segs:
        raise SystemExit("❌ 플래시 영역 PT_LOAD 세그먼트가 없다")
    return sorted(segs)


def to_bin(data, segs):
    base = segs[0][0]
    end  = max(pa + sz for pa, _, sz in segs)
    size = end - base
    if size > 1024 * 1024:
        raise SystemExit(f"❌ 크기 이상: {size:,} B — 세그먼트 선별 오류")
    buf = bytearray(b'\xFF' * size)
    for pa, off, sz in segs:
        buf[pa - base:pa - base + sz] = data[off:off + sz]
    return base, bytes(buf)


def to_ihex(payload, base):
    out, ext = [], -1
    for i in range(0, len(payload), 16):
        chunk = payload[i:i + 16]
        addr = base + i
        hi = (addr >> 16) & 0xFFFF
        if hi != ext:                       # 확장 선형 주소 레코드
            rec = [2, 0, 0, 4, (hi >> 8) & 0xFF, hi & 0xFF]
            rec.append((-sum(rec)) & 0xFF)
            out.append(':' + ''.join(f'{b:02X}' for b in rec))
            ext = hi
        lo = addr & 0xFFFF
        rec = [len(chunk), (lo >> 8) & 0xFF, lo & 0xFF, 0] + list(chunk)
        rec.append((-sum(rec)) & 0xFF)
        out.append(':' + ''.join(f'{b:02X}' for b in rec))
    out.append(':00000001FF')               # EOF
    return '\n'.join(out) + '\n'


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    elf, prefix = sys.argv[1], sys.argv[2]
    data = open(elf, 'rb').read()
    segs = load_segments(data)
    base, payload = to_bin(data, segs)

    open(prefix + '.bin', 'wb').write(payload)
    open(prefix + '.hex', 'w').write(to_ihex(payload, base))

    sp, reset = struct.unpack_from('<II', payload, 0)
    print(f"   base   0x{base:08x}   size {len(payload):,} B")
    print(f"   SP     0x{sp:08x}")
    print(f"   Reset  0x{reset:08x}  (thumb {'OK' if reset & 1 else 'FAIL'})")
    if not reset & 1:
        raise SystemExit("❌ Reset 벡터의 thumb 비트가 0 — 부팅 불가")


if __name__ == '__main__':
    main()
