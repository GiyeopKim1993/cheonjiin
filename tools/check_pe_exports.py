#!/usr/bin/env python3
"""PE(DLL) 내보내기 테이블 검사.

Windows 없이 DLL 이 제대로 만들어졌는지 확인한다. 컴파일·링크가 성공해도
COM 진입점이 내보내기 테이블에 없으면 regsvr32 가 실패하는데, 실제로
그 상태의 DLL 이 나온 적이 있어 이 검사를 CI 에 넣는다.

사용법: python3 tools/check_pe_exports.py <dll> [필수심볼...]
"""
import struct, sys


def exports(path):
    d = open(path, 'rb').read()
    if d[:2] != b'MZ':
        raise SystemExit(f"❌ {path}: MZ 헤더 없음 (PE 파일이 아님)")
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    if d[pe:pe + 4] != b'PE\0\0':
        raise SystemExit(f"❌ {path}: PE 시그니처 없음")

    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    optsz = struct.unpack_from('<H', d, pe + 20)[0]
    chars = struct.unpack_from('<H', d, pe + 22)[0]
    magic = struct.unpack_from('<H', d, pe + 24)[0]
    is_dll = bool(chars & 0x2000)
    arch = {0x20b: 'x86-64', 0x10b: 'x86'}.get(magic, hex(magic))

    ddoff = pe + 24 + (112 if magic == 0x20b else 96)
    erva = struct.unpack_from('<I', d, ddoff)[0]

    secs = []
    for i in range(nsec):
        o = pe + 24 + optsz + i * 40
        vs, va, rs, pr = struct.unpack_from('<IIII', d, o + 8)
        secs.append((va, vs, pr, rs))

    def r2o(rva):
        for va, vs, pr, rs in secs:
            if va <= rva < va + max(vs, rs):
                return pr + (rva - va)
        return None

    names = []
    if erva:
        eo = r2o(erva)
        nnam = struct.unpack_from('<I', d, eo + 24)[0]
        anam = struct.unpack_from('<I', d, eo + 32)[0]
        for i in range(nnam):
            nr = struct.unpack_from('<I', d, r2o(anam) + i * 4)[0]
            o = r2o(nr)
            names.append(d[o:d.index(b'\0', o)].decode())
    return names, is_dll, arch, len(d)


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    path = sys.argv[1]
    need = sys.argv[2:] or [
        'DllGetClassObject', 'DllCanUnloadNow',
        'DllRegisterServer', 'DllUnregisterServer',
    ]
    names, is_dll, arch, size = exports(path)
    print(f"{path}: PE {arch} {'DLL' if is_dll else '실행파일'} "
          f"/ {size:,} B / 내보내기 {len(names)}개")
    if not is_dll:
        raise SystemExit("❌ DLL 플래그가 없다")
    missing = [n for n in need if n not in names]
    for n in need:
        print(f"  {'✅' if n not in missing else '❌'} {n}")
    if missing:
        raise SystemExit(f"❌ 내보내기 누락: {missing}\n"
                         "   STDAPI 만으로는 부족하다. __declspec(dllexport) 필요.")
    print("✅ COM 진입점 전부 존재 — regsvr32 등록 가능")


if __name__ == '__main__':
    main()
