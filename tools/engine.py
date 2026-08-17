"""
천지인 IME 참조 조합 엔진 (Reference Implementation)
- 순수 함수형 상태 머신. 플랫폼(Win/macOS/Linux/Android/iOS) 비의존.
- 이 파일은 스펙 문서의 '정답지' 역할을 하며, 모든 표는 이 코드 실행 결과로 생성한다.
"""
from dataclasses import dataclass, field, replace
from typing import Optional, List, Tuple

# ---------------------------------------------------------------- 유니코드 테이블
CHO = "ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ"
JUNG = "ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ"
JONG = " ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ"

def compose(cho: str, jung: str, jong: str = "") -> str:
    return chr(0xAC00 + (CHO.index(cho) * 21 + JUNG.index(jung)) * 28 + JONG.index(jong or " "))

# ---------------------------------------------------------------- 키맵 (12키)
# K1..K9, KSTAR(획추가), K0, KHASH(쌍자음)
VOWEL_KEYS = {"K1": "ㅣ", "K2": "ㆍ", "K3": "ㅡ"}

# 자음 멀티탭 순환열
CONSONANT_CYCLE = {
    "K4": ["ㄱ", "ㅋ", "ㄲ"],
    "K5": ["ㄴ", "ㄹ"],
    "K6": ["ㄷ", "ㅌ", "ㄸ"],
    "K7": ["ㅂ", "ㅍ", "ㅃ"],
    "K8": ["ㅅ", "ㅎ", "ㅆ"],
    "K9": ["ㅈ", "ㅊ", "ㅉ"],
    "K0": ["ㅇ", "ㅁ"],
}
# 획추가(*) / 쌍자음(#) 결정적 매핑 — 멀티탭 대체 경로
STROKE_ADD = {"ㄱ": "ㅋ", "ㄴ": "ㄷ", "ㄷ": "ㅌ", "ㄹ": "ㅌ", "ㅁ": "ㅂ", "ㅂ": "ㅍ",
              "ㅅ": "ㅈ", "ㅈ": "ㅊ", "ㅇ": "ㅎ", "ㅋ": "ㄱ", "ㅌ": "ㄷ", "ㅍ": "ㅂ",
              "ㅊ": "ㅅ", "ㅎ": "ㅇ"}
DOUBLE = {"ㄱ": "ㄲ", "ㄷ": "ㄸ", "ㅂ": "ㅃ", "ㅅ": "ㅆ", "ㅈ": "ㅉ",
          "ㅋ": "ㄲ", "ㅌ": "ㄸ", "ㅍ": "ㅃ", "ㅎ": "ㅆ", "ㅊ": "ㅉ",
          "ㄲ": "ㄱ", "ㄸ": "ㄷ", "ㅃ": "ㅂ", "ㅆ": "ㅅ", "ㅉ": "ㅈ"}

# ---------------------------------------------------------------- 모음 오토마타
# state -> {입력자모: 다음 state}. state 'ㆍ','ㆍㆍ'는 미완성(비출력) 중간 상태.
V: dict[str, dict[str, str]] = {
    "":     {"ㅣ": "ㅣ",  "ㆍ": "ㆍ",  "ㅡ": "ㅡ"},
    "ㅣ":   {"ㆍ": "ㅏ"},
    "ㆍ":   {"ㅣ": "ㅓ",  "ㅡ": "ㅗ",  "ㆍ": "ㆍㆍ"},
    "ㆍㆍ": {"ㅣ": "ㅕ",  "ㅡ": "ㅛ",  "ㆍ": "ㆍ"},      # 3타는 1타로 순환
    "ㅡ":   {"ㆍ": "ㅜ",  "ㅣ": "ㅢ"},
    "ㅏ":   {"ㆍ": "ㅑ",  "ㅣ": "ㅐ"},
    "ㅑ":   {"ㅣ": "ㅒ",  "ㆍ": "ㅏ"},                   # ㆍ 순환
    "ㅓ":   {"ㅣ": "ㅔ"},
    "ㅕ":   {"ㅣ": "ㅖ"},
    "ㅐ":   {"ㆍ": "ㅒ"},                                # ㅣㆍㅣㆍ 대체 경로
    "ㅗ":   {"ㅣ": "ㅚ"},
    "ㅚ":   {"ㆍ": "ㅘ"},
    "ㅘ":   {"ㅣ": "ㅙ"},
    "ㅜ":   {"ㅣ": "ㅟ",  "ㆍ": "ㅠ"},
    "ㅠ":   {"ㅣ": "ㅝ"},
    "ㅝ":   {"ㅣ": "ㅞ"},
    "ㅟ":   {"ㆍ": "ㅝ"},                                # ㅡㆍㅣㆍ 대체 경로
    "ㅛ":   {}, "ㅒ": {}, "ㅔ": {"ㅣ": ""}, "ㅖ": {}, "ㅙ": {}, "ㅞ": {}, "ㅢ": {},
}
# 미완성(화면에 모음으로 확정 출력되지 않는) 상태
PENDING = {"ㆍ", "ㆍㆍ"}

# 겹받침 결합
JONG_COMBINE = {("ㄱ", "ㅅ"): "ㄳ", ("ㄴ", "ㅈ"): "ㄵ", ("ㄴ", "ㅎ"): "ㄶ",
                ("ㄹ", "ㄱ"): "ㄺ", ("ㄹ", "ㅁ"): "ㄻ", ("ㄹ", "ㅂ"): "ㄼ",
                ("ㄹ", "ㅅ"): "ㄽ", ("ㄹ", "ㅌ"): "ㄾ", ("ㄹ", "ㅍ"): "ㄿ",
                ("ㄹ", "ㅎ"): "ㅀ", ("ㅂ", "ㅅ"): "ㅄ"}
JONG_SPLIT = {v: k for k, v in JONG_COMBINE.items()}
VALID_JONG = set(JONG.strip())


@dataclass
class State:
    cho: str = ""
    vstate: str = ""      # 모음 오토마타 상태
    jong: str = ""
    last_key: Optional[str] = None   # 멀티탭 대상 키
    tap: int = 0                     # 멀티탭 인덱스
    snap: Optional[tuple] = None     # 멀티탭 시작 직전 상태 스냅샷
    committed: str = ""              # 확정 출력 문자열
    slot: str = "cho"                # 'cho' | 'jung' | 'jong'

    def preedit(self) -> str:
        """조합 중 글자(화면에 밑줄로 표시되는 부분)"""
        v = "" if self.vstate in PENDING else self.vstate
        if self.cho and v:
            return compose(self.cho, v, self.jong)
        if self.cho:
            return self.cho
        if v:
            return v
        return ""

    def text(self) -> str:
        return self.committed + self.preedit()


def _flush(s: State) -> State:
    return replace(s, committed=s.committed + s.preedit(), cho="", vstate="", jong="",
                   last_key=None, tap=0, snap=None, slot="cho")



def _snapshot(s: State) -> tuple:
    return (s.cho, s.vstate, s.jong, s.committed, s.slot)


def _restore(s: State, snap: tuple) -> State:
    cho, vstate, jong, committed, slot = snap
    return replace(s, cho=cho, vstate=vstate, jong=jong, committed=committed, slot=slot)


def _attach(s: State, c: str) -> State:
    """자음 c를 현재 조합 상태에 붙인다 (멀티탭 tap 관리는 호출측 책임)."""
    # 빈 초성 자리
    if s.slot == "cho" and not s.cho:
        return replace(s, cho=c, slot="cho")
    # 종성이 이미 있음 -> 겹받침 결합 시도
    if s.slot == "jong" and s.jong:
        comb = JONG_COMBINE.get((s.jong, c))
        if comb:
            return replace(s, jong=comb, slot="jong")
        s = _flush(s)
        return replace(s, cho=c, slot="cho")
    # 초성+중성 완성 -> 종성 자리
    if s.cho and s.vstate and s.vstate not in PENDING:
        if c in VALID_JONG:
            return replace(s, jong=c, slot="jong")
        s = _flush(s)
        return replace(s, cho=c, slot="cho")
    # 그 외 -> 확정 후 새 글자
    s = _flush(s)
    return replace(s, cho=c, slot="cho")


def press(s: State, key: str) -> State:
    # ---------------- 모음 키
    if key in VOWEL_KEYS:
        jamo = VOWEL_KEYS[key]
        # 받침이 있는 상태에서 모음 -> 받침을 다음 글자 초성으로 이동 (연음/도깨비불)
        if s.jong:
            moved, rest = _pull_jong(s.jong)
            base = replace(s, jong=rest)
            base = _flush(base)
            s = replace(base, cho=moved, slot="jung")
        nxt = V.get(s.vstate, {}).get(jamo)
        if nxt is None:
            # 현재 상태에서 이어붙일 수 없음 -> 확정 후 새 글자 시작
            s = _flush(s)
            nxt = V[""][jamo]
        return replace(s, vstate=nxt, last_key=None, tap=0, snap=None, slot="jung")

    # ---------------- 자음 키
    if key in CONSONANT_CYCLE:
        cyc = CONSONANT_CYCLE[key]
        if s.last_key == key and s.snap is not None:
            tap = (s.tap + 1) % len(cyc)
            base = _restore(s, s.snap)
        else:
            tap = 0
            base = s
        snap = _snapshot(base)
        out = _attach(base, cyc[tap])
        return replace(out, last_key=key, tap=tap, snap=snap)

    # ---------------- 획추가(*) / 쌍자음(#)
    if key in ("KSTAR", "KHASH"):
        table = STROKE_ADD if key == "KSTAR" else DOUBLE
        # 1) 종성 자리 (겹받침이면 뒤 성분에 적용)
        if s.slot == "jong" and s.jong:
            if s.jong in JONG_SPLIT:
                head, tail = JONG_SPLIT[s.jong]
                nt = table.get(tail)
                if nt:
                    comb = JONG_COMBINE.get((head, nt))
                    if comb:
                        return replace(s, jong=comb, last_key=None, snap=None)
                return s
            nc = table.get(s.jong)
            if nc and nc in VALID_JONG:
                return replace(s, jong=nc, last_key=None, snap=None)
            return s
        # 2) 초성 자리
        if s.slot == "cho" and s.cho:
            nc = table.get(s.cho)
            if nc:
                return replace(s, cho=nc, last_key=None, snap=None)
            return s
        # 3) 모음에 대한 획추가 = ㆍ 부가 (쌍자음 키는 모음에 무효)
        if key == "KSTAR" and s.vstate:
            nxt = V.get(s.vstate, {}).get("ㆍ")
            if nxt:
                return replace(s, vstate=nxt, last_key=None, snap=None)
        return s

    # ---------------- 공백 / 백스페이스 / 확정
    # 오른쪽 방향키: 조합 중이면 확정(커밋)만, 아니면 커서 이동은 셸 담당
    if key == "KRIGHT":
        if s.preedit():
            return _flush(s)
        return replace(s, last_key=None, tap=0, snap=None)
    # 왼쪽 방향키: 조합 중이면 확정 후 셸이 커서 이동
    if key == "KLEFT":
        if s.preedit():
            return _flush(s)
        return replace(s, last_key=None, tap=0, snap=None)
    if key == "TIMEOUT":
        return replace(s, last_key=None, tap=0, snap=None)
    if key == "SPACE":
        s = _flush(s)
        return replace(s, committed=s.committed + " ")
    if key == "BACK":
        return backspace(s)
    if key == "COMMIT":
        return _flush(s)
    raise ValueError(key)


def _pull_jong(jong: str) -> Tuple[str, str]:
    """받침에서 다음 초성으로 옮길 자모와 남는 받침"""
    if jong in JONG_SPLIT:
        a, b = JONG_SPLIT[jong]
        return b, a
    return jong, ""


def backspace(s: State) -> State:
    """자모 단위 역순 삭제 (조합 역추적)"""
    if s.jong:
        if s.jong in JONG_SPLIT:
            return replace(s, jong=JONG_SPLIT[s.jong][0], last_key=None, tap=0, snap=None)
        return replace(s, jong="", last_key=None, tap=0, snap=None, slot="jung")
    if s.vstate:
        prev = _prev_vstate(s.vstate)
        return replace(s, vstate=prev, last_key=None, tap=0, snap=None,
                       slot="jung" if prev else "cho")
    if s.cho:
        return replace(s, cho="", last_key=None, tap=0, snap=None, slot="cho")
    if s.committed:
        return replace(s, committed=s.committed[:-1])
    return s


_PREV = {}
def _build_prev():
    for st, tr in V.items():
        for jamo, nxt in tr.items():
            if nxt and nxt not in _PREV:
                _PREV[nxt] = st
_build_prev()

def _prev_vstate(v: str) -> str:
    return _PREV.get(v, "")


def type_keys(seq: List[str]) -> str:
    s = State()
    for k in seq:
        s = press(s, k)
    return _flush(s).committed


# ---------------------------------------------------------------- 역방향: 문자 -> 키시퀀스
KEY_OF_JAMO = {}
for k, cyc in CONSONANT_CYCLE.items():
    for i, c in enumerate(cyc):
        KEY_OF_JAMO.setdefault(c, (k, i + 1))

def vowel_seq(v: str) -> List[str]:
    """모음 -> 최단 키 시퀀스 (BFS)"""
    from collections import deque
    inv = {jamo: k for k, jamo in VOWEL_KEYS.items()}
    q = deque([("", [])])
    seen = {""}
    while q:
        st, path = q.popleft()
        if st == v:
            return path
        for jamo, nxt in V.get(st, {}).items():
            if nxt and nxt not in seen:
                seen.add(nxt)
                q.append((nxt, path + [inv[jamo]]))
    return []
