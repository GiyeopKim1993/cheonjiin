#!/usr/bin/env python3
"""단어 사전 전수 검수 (docs 4단계 c)

29,361개 단어를 기계적으로 전수 검증한다. 핵심 불변식:

  **단어의 각 음절은, 대응하는 한자의 한국 한자음과 일치해야 한다.**

이 불변식이 깨지면 사용자가 '가정'을 치고 家庭 대신 엉뚱한 한자를 받는다.
Unihan kHangul(한국 한자음 권위 데이터)로 대조한다.

검사 항목:
  1. 길이 불일치        — 한글 음절 수 != 한자 수
  2. 음가 불일치        — 한자의 kHangul 음에 해당 음절이 없음
  3. 비한자 혼입        — 한자 필드에 한글/라틴/기호
  4. 표제어 이상        — 한글 아닌 문자 포함
  5. 중복/자기참조
  6. 두음법칙 예외 처리 — 랑->낭, 려->여 등 (오탐 방지)

사용법:
  python3 tools/audit_dict.py [Unihan_Readings.txt]
  python3 tools/audit_dict.py --fix     # 오류 항목 제거본 생성
"""
import json, sys, os, re, unicodedata
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DICT = os.path.join(ROOT, 'spec', 'hanja-dict.json')

# ── 두음법칙 (한국어 표기 규칙) ──
# 한자음은 본음이지만 어두에서 달리 적는다. kHangul 은 본음을 주므로
# 이걸 반영하지 않으면 정상 단어를 오류로 잡는다.
INITIAL_L = {  # ㄹ -> ㄴ/ㅇ
    '라': '나', '락': '낙', '란': '난', '람': '남', '랑': '낭', '래': '내',
    '랭': '냉', '로': '노', '록': '녹', '론': '논', '롱': '농', '뢰': '뇌',
    '료': '요', '루': '누', '류': '유', '륙': '육', '륜': '윤', '률': '율',
    '륭': '융', '르': '느', '름': '늠', '릉': '능', '리': '이', '린': '인',
    '림': '임', '립': '입', '량': '양', '려': '여', '력': '역', '련': '연',
    '렬': '열', '렴': '염', '렵': '엽', '령': '영', '례': '예', '로': '노',
    '롱': '농', '뇨': '요', '뉴': '유', '니': '이',
}
INITIAL_N = {  # ㄴ -> ㅇ (녀->여, 뇨->요, 뉴->유, 니->이)
    '녀': '여', '뇨': '요', '뉴': '유', '니': '이', '녕': '영',
}


def load_khangul(path):
    """Unihan_Readings.txt -> {코드포인트: {한국음...}}"""
    m = defaultdict(set)
    with open(path, encoding='utf-8') as f:
        for line in f:
            if not line.startswith('U+'):
                continue
            parts = line.rstrip('\n').split('\t')
            if len(parts) < 3 or parts[1] != 'kHangul':
                continue
            cp = int(parts[0][2:], 16)
            # "가:0E 각:0E" 형태 — 콜론 뒤는 출처 태그
            for tok in parts[2].split():
                syl = tok.split(':')[0]
                if syl:
                    m[cp].add(syl)
    return m


def variants(syl):
    """두음법칙 등으로 허용되는 표기 변형 집합"""
    out = {syl}
    for table in (INITIAL_L, INITIAL_N):
        for base, alt in table.items():
            if syl == alt:
                out.add(base)      # 표기(낭) <- 본음(랑)
            if syl == base:
                out.add(alt)
    return out


def is_hangul_syllable(ch):
    return 0xAC00 <= ord(ch) <= 0xD7A3


def is_han(ch):
    cp = ord(ch)
    return (0x4E00 <= cp <= 0x9FFF or 0x3400 <= cp <= 0x4DBF
            or 0xF900 <= cp <= 0xFAFF or 0x20000 <= cp <= 0x2FA1F)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    do_fix = '--fix' in sys.argv
    uni = args[0] if args else '/tmp/Unihan_Readings.txt'
    if not os.path.exists(uni):
        raise SystemExit(f"❌ Unihan_Readings.txt 없음: {uni}\n"
                         "   curl -sSL -o /tmp/Unihan.zip "
                         "https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip "
                         "&& (cd /tmp && unzip -o Unihan.zip Unihan_Readings.txt)")

    kh = load_khangul(uni)
    d = json.load(open(DICT, encoding='utf-8'))
    words = d['words']
    chars = d['chars']

    errs = defaultdict(list)

    for kor, han in words.items():
        # 4. 표제어는 한글 음절만
        if not all(is_hangul_syllable(c) for c in kor):
            errs['표제어이상'].append((kor, han, '한글 아닌 문자'))
            continue
        # 3. 한자 필드는 한자만
        bad = [c for c in han if not is_han(c)]
        if bad:
            errs['비한자혼입'].append((kor, han, ''.join(bad)))
            continue
        # 1. 길이 일치
        if len(kor) != len(han):
            errs['길이불일치'].append((kor, han, f'{len(kor)}!={len(han)}'))
            continue
        # 2. 음가 대조
        for i, (ks, hc) in enumerate(zip(kor, han)):
            readings = kh.get(ord(hc))
            if not readings:
                errs['음가없음'].append((kor, han, f'{hc}(U+{ord(hc):04X}) kHangul 없음'))
                break
            if not (variants(ks) & readings):
                errs['음가불일치'].append(
                    (kor, han, f'{i}번째 {ks}≠{hc}({"/".join(sorted(readings))})'))
                break
        # 5. 자기참조
        if kor == han:
            errs['자기참조'].append((kor, han, ''))

    # chars 맵도 같은 방식으로 검증
    char_err = []
    for syl, lst in chars.items():
        for hc in lst:
            readings = kh.get(ord(hc))
            if readings and not (variants(syl) & readings):
                char_err.append((syl, hc, '/'.join(sorted(readings))))

    total_err = sum(len(v) for v in errs.values())
    print(f"단어 {len(words):,}개 / 음절맵 {len(chars)}개 전수 검수")
    print(f"{'─' * 60}")
    for k in ('표제어이상', '비한자혼입', '길이불일치', '음가없음', '음가불일치', '자기참조'):
        v = errs.get(k, [])
        mark = '✅' if not v else '❌'
        print(f"{mark} {k:10s} {len(v):5,}건")
        for kor, han, why in v[:8]:
            print(f"      {kor} → {han}   ({why})")
        if len(v) > 8:
            print(f"      … 외 {len(v) - 8:,}건")
    print(f"{'✅' if not char_err else '❌'} 음절맵음가   {len(char_err):5,}건")
    for syl, hc, r in char_err[:8]:
        print(f"      {syl} → {hc} (실제음 {r})")
    if len(char_err) > 8:
        print(f"      … 외 {len(char_err) - 8:,}건")

    print(f"{'─' * 60}")
    print(f"단어 오류 합계: {total_err:,}건 "
          f"({total_err / max(len(words), 1) * 100:.3f}%)")

    if do_fix:
        drop = set()
        for k in ('표제어이상', '비한자혼입', '길이불일치', '음가없음',
                  '음가불일치', '자기참조'):
            for kor, han, _ in errs.get(k, []):
                drop.add(kor)
        clean = {k: v for k, v in words.items() if k not in drop}
        clean_chars = {}
        for syl, lst in chars.items():
            keep = [h for h in lst
                    if not kh.get(ord(h)) or (variants(syl) & kh[ord(h)])]
            if keep:
                clean_chars[syl] = keep
        d['words'] = clean
        d['chars'] = clean_chars
        d['wordCount'] = len(clean)
        d['syllableCount'] = len(clean_chars)
        d['charCount'] = sum(len(v) for v in clean_chars.values())
        d['audit'] = {
            'method': 'Unihan kHangul 음가 대조 + 두음법칙 변형 허용',
            'removedWords': len(drop),
            'removedCharMappings': sum(len(v) for v in chars.values())
                                   - d['charCount'],
        }
        with open(DICT, 'w', encoding='utf-8') as f:
            json.dump(d, f, ensure_ascii=False, separators=(',', ':'))
        print(f"\n✅ 정제본 저장: 단어 {len(words):,} → {len(clean):,} "
              f"(제거 {len(drop):,}), 한자매핑 {d['charCount']:,}")
    return 0 if total_err == 0 and not char_err else 1


if __name__ == '__main__':
    sys.exit(main())
