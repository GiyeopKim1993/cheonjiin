"""텍스트 -> 천지인 키 시퀀스 인코더 (왕복 검증 및 타수 측정용)"""
from engine import (CHO, JUNG, JONG, JONG_SPLIT, CONSONANT_CYCLE, vowel_seq,
                    VALID_JONG)

KEY_OF_JAMO = {}
for k, cyc in CONSONANT_CYCLE.items():
    for i, c in enumerate(cyc):
        KEY_OF_JAMO.setdefault(c, (k, i + 1))


def cons_seq(c: str):
    k, taps = KEY_OF_JAMO[c]
    return [k] * taps, k


def decompose(ch: str):
    code = ord(ch) - 0xAC00
    return CHO[code // 588], JUNG[(code % 588) // 28], JONG[code % 28].strip()


def encode_syllable(ch: str):
    """한 음절 -> 키 시퀀스. 멀티탭 충돌 지점에는 TIMEOUT 삽입."""
    cho, jung, jong = decompose(ch)
    seq, last_key = [], None

    cs, k = cons_seq(cho)
    seq += cs
    last_key = k

    vs = vowel_seq(jung)
    seq += vs
    last_key = None  # 모음 입력이 멀티탭 세션을 끊음

    if jong:
        parts = list(JONG_SPLIT[jong]) if jong in JONG_SPLIT else [jong]
        for p in parts:
            cs, k = cons_seq(p)
            if k == last_key:
                seq.append("TIMEOUT")
            seq += cs
            last_key = k
    return seq, last_key


def encode(text: str):
    seq, last_key = [], None
    for ch in text:
        if ch == " ":
            seq.append("SPACE")
            last_key = None
            continue
        s, lk = encode_syllable(ch)
        # 앞 음절의 끝 키와 이번 음절의 첫 키가 같으면 멀티탭 충돌 -> TIMEOUT
        if s and s[0] == last_key:
            seq.append("TIMEOUT")
        seq += s
        last_key = lk
    return seq
