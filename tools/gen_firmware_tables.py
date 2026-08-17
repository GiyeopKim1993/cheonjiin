#!/usr/bin/env python3
"""
검증된 참조 구현(tools/engine.py)에서 펌웨어용 C 테이블을 생성한다.

손으로 옮기면 드리프트가 생기므로 반드시 생성한다.
출력: firmware/core/cuime_tables.h
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine import (CHO, JUNG, JONG, V, PENDING, VOWEL_KEYS, CONSONANT_CYCLE,
                    JONG_COMBINE, JONG_SPLIT)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ── 모음 상태 열거: 0=없음, 1..21=완성 모음(JUNG 순), 22=ㆍ, 23=ㆍㆍ
VSTATES = [""] + list(JUNG) + ["ㆍ", "ㆍㆍ"]
VIDX = {s: i for i, s in enumerate(VSTATES)}
VKEY_ORDER = ["ㅣ", "ㆍ", "ㅡ"]          # K1, K2, K3

# ── 자음 키 순환 (K4..K9, K0 = 인덱스 0..6)
CKEYS = ["K4", "K5", "K6", "K7", "K8", "K9", "K0"]

def u(ch):
    return f"0x{ord(ch):04X}"

out = []
w = out.append

w("/* 천지인 펌웨어 조합 테이블 — 자동 생성 (tools/gen_firmware_tables.py)")
w(" * 직접 수정 금지. 참조 구현(tools/engine.py)에서 생성됨.")
w(" *")
w(" * 모든 테이블은 const 이므로 MCU 플래시(.rodata)에 놓인다. RAM 소비 0.")
w(" */")
w("#ifndef CUIME_TABLES_H")
w("#define CUIME_TABLES_H")
w("")
w("#include <stdint.h>")
w("")

# 초성/중성/종성 유니코드 자모
w(f"#define CUIME_CHO_COUNT  {len(CHO)}")
w(f"#define CUIME_JUNG_COUNT {len(JUNG)}")
w(f"#define CUIME_JONG_COUNT {len(JONG)}")
w("")
w("/* 초성 19 (조합용 인덱스 순서) */")
w("static const uint16_t CUIME_CHO[CUIME_CHO_COUNT] = {")
w("    " + ", ".join(u(c) for c in CHO))
w("};")
w("")
w("/* 중성 21 */")
w("static const uint16_t CUIME_JUNG[CUIME_JUNG_COUNT] = {")
w("    " + ", ".join(u(c) for c in JUNG))
w("};")
w("")
w("/* 종성 28 (0 = 받침 없음) */")
w("static const uint16_t CUIME_JONG[CUIME_JONG_COUNT] = {")
w("    0x0000, " + ", ".join(u(c) for c in JONG[1:]))
w("};")
w("")

# 모음 오토마타
w(f"#define CUIME_VSTATE_COUNT {len(VSTATES)}")
w("#define CUIME_VS_NONE 0")
w(f"#define CUIME_VS_DOT  {VIDX['ㆍ']}   /* 미완성: ㆍ */")
w(f"#define CUIME_VS_DOT2 {VIDX['ㆍㆍ']}  /* 미완성: ㆍㆍ */")
w("")
w("/* 모음 상태 -> 확정 중성 인덱스(0-base). 0xFF = 미완성(화면 미출력) */")
w("static const uint8_t CUIME_VSTATE_JUNG[CUIME_VSTATE_COUNT] = {")
row = []
for s in VSTATES:
    if s == "" or s in PENDING:
        row.append("0xFF")
    else:
        row.append(str(JUNG.index(s)))
w("    " + ", ".join(row))
w("};")
w("")
w("/* 전이표: [현재상태][키(0=ㅣ,1=ㆍ,2=ㅡ)] -> 다음상태. 0xFF = 전이 없음 */")
w("static const uint8_t CUIME_VTRANS[CUIME_VSTATE_COUNT][3] = {")
for s in VSTATES:
    tr = V.get(s, {})
    cells = []
    for jamo in VKEY_ORDER:
        nxt = tr.get(jamo)
        cells.append(str(VIDX[nxt]) if nxt is not None else "0xFF")
    w(f"    {{ {', '.join(cells):>16} }},   /* {s or '(none)'} */")
w("};")
w("")
w("/* 백스페이스 역추적: 상태 -> 이전 상태. 0xFF = 없음")
w(" * 참조 구현(engine._PREV)을 그대로 가져온다. 순회 순서에 의존하므로")
w(" * 재구현하면 어긋난다(실제로 겪은 버그). */")
from engine import _PREV as prev
w("static const uint8_t CUIME_VPREV[CUIME_VSTATE_COUNT] = {")
w("    " + ", ".join(
    ("0xFF" if s == "" else str(VIDX[prev[s]]) if s in prev else "0xFF")
    for s in VSTATES))
w("};")
w("")

# 자음 순환
w(f"#define CUIME_CKEY_COUNT {len(CKEYS)}")
w("#define CUIME_CYCLE_MAX 3")
w("")
w("/* 자음 키별 순환열 (유니코드 자모). 0 = 빈 슬롯 */")
w("static const uint16_t CUIME_CYCLE[CUIME_CKEY_COUNT][CUIME_CYCLE_MAX] = {")
for k in CKEYS:
    cyc = CONSONANT_CYCLE[k]
    cells = [u(c) for c in cyc] + ["0x0000"] * (3 - len(cyc))
    w(f"    {{ {', '.join(cells)} }},   /* {k}: {'→'.join(cyc)} */")
w("};")
w("")
w("static const uint8_t CUIME_CYCLE_LEN[CUIME_CKEY_COUNT] = {")
w("    " + ", ".join(str(len(CONSONANT_CYCLE[k])) for k in CKEYS))
w("};")
w("")
w("/* 롱프레스 = 순환열 마지막 항목 (docs/00 §3.2) */")
w("static const uint16_t CUIME_LONGPRESS[CUIME_CKEY_COUNT] = {")
w("    " + ", ".join(u(CONSONANT_CYCLE[k][-1]) for k in CKEYS))
w("};")
w("")

# 겹받침
pairs = sorted(JONG_SPLIT.items(), key=lambda x: JONG.index(x[0]))
w(f"#define CUIME_CLUSTER_COUNT {len(pairs)}")
w("/* 겹받침: {합성 종성, 앞 자모, 뒤 자모} */")
w("static const uint16_t CUIME_CLUSTER[CUIME_CLUSTER_COUNT][3] = {")
for comb, (a, b) in pairs:
    w(f"    {{ {u(comb)}, {u(a)}, {u(b)} }},   /* {comb} = {a}+{b} */")
w("};")
w("")

# 두벌식 매핑 (HID 출력용): 자모 -> QWERTY 문자
DUBEOL = {
    'ㅂ':'q','ㅈ':'w','ㄷ':'e','ㄱ':'r','ㅅ':'t','ㅛ':'y','ㅕ':'u','ㅑ':'i','ㅐ':'o','ㅔ':'p',
    'ㅁ':'a','ㄴ':'s','ㅇ':'d','ㄹ':'f','ㅎ':'g','ㅗ':'h','ㅓ':'j','ㅏ':'k','ㅣ':'l',
    'ㅋ':'z','ㅌ':'x','ㅊ':'c','ㅍ':'v','ㅠ':'b','ㅜ':'n','ㅡ':'m',
    'ㅃ':'Q','ㅉ':'W','ㄸ':'E','ㄲ':'R','ㅆ':'T','ㅒ':'O','ㅖ':'P',
}
# 복합 모음은 두벌식에서 두 타로 분해
COMPLEX_V = {'ㅘ':'ㅗㅏ','ㅙ':'ㅗㅐ','ㅚ':'ㅗㅣ','ㅝ':'ㅜㅓ','ㅞ':'ㅜㅔ','ㅟ':'ㅜㅣ','ㅢ':'ㅡㅣ'}

w("/* ── 두벌식 HID 매핑 ──")
w(" * BT/USB HID 키보드는 유니코드가 아니라 키코드를 보낸다.")
w(" * 따라서 기기가 조합한 자모를 두벌식 자판 위치로 변환해 전송하고,")
w(" * 호스트의 한글 IME가 음절로 합친다. (docs/10 §4)")
w(" * 값: ASCII 문자 (대문자 = Shift 필요). 0 = 매핑 없음.")
w(" */")
w("typedef struct { uint16_t jamo; char a; char b; } cuime_dubeol_t;")
entries = []
for jm, ch in DUBEOL.items():
    entries.append((jm, ch, '\\0'))
for jm, dec in COMPLEX_V.items():
    entries.append((jm, DUBEOL[dec[0]], DUBEOL[dec[1]]))
# 겹받침도 두 타로
for comb, (a, b) in pairs:
    entries.append((comb, DUBEOL[a], DUBEOL[b]))
entries.sort(key=lambda e: ord(e[0]))
w(f"#define CUIME_DUBEOL_COUNT {len(entries)}")
w("static const cuime_dubeol_t CUIME_DUBEOL[CUIME_DUBEOL_COUNT] = {")
for jm, a, b in entries:
    bs = "0" if b == '\\0' else f"'{b}'"
    w(f"    {{ {u(jm)}, '{a}', {bs} }},   /* {jm} */")
w("};")
w("")
w("#endif /* CUIME_TABLES_H */")

dst = f"{ROOT}/firmware/core/cuime_tables.h"
os.makedirs(os.path.dirname(dst), exist_ok=True)
open(dst, "w").write("\n".join(out) + "\n")
print(f"생성: {dst}  ({os.path.getsize(dst):,} bytes)")
print(f"  모음 상태 {len(VSTATES)} / 자음 키 {len(CKEYS)} / 겹받침 {len(pairs)} / 두벌식 {len(entries)}")
