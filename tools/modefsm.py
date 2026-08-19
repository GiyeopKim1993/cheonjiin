"""
한자/기호 키 + 방향키 코드(chord) 및 레이아웃 전환 상태 머신 — 참조 구현.
셸(플랫폼) 계층 로직이며, 조합 엔진(engine.py)과 분리된다.

이벤트 모델: 키를 down/up으로 분해해 받는다 (코드 판정에 필수).
"""
from dataclasses import dataclass, field, replace
from typing import List, Optional

LAYOUTS = ["한글", "영어", "숫자"]


@dataclass
class ShellState:
    layout: str = "한글"
    hj_down: bool = False          # 한자/기호 키 눌림
    hj_consumed: bool = False      # 코드로 소비됨 -> release 시 팔레트 안 뜸
    candidates_open: bool = False  # 한자 후보 창 열림
    palette_open: bool = False     # 기호 팔레트 열림
    cand_index: int = 0
    log: List[str] = field(default_factory=list)


def _cycle(layout: str, delta: int) -> str:
    return LAYOUTS[(LAYOUTS.index(layout) + delta) % len(LAYOUTS)]


def handle(s: ShellState, ev: str, *, has_selection: bool = False,
           selection_is_hangul: bool = False, composing: bool = False) -> ShellState:
    """
    ev: 'HJ_DOWN','HJ_UP','LEFT_DOWN','RIGHT_DOWN','ESC'
    has_selection: 단어/글자 블록(선택) 여부
    """
    log = list(s.log)

    # ---------- 한자/기호 키 ----------
    if ev == "HJ_DOWN":
        return replace(s, hj_down=True, hj_consumed=False, log=log)

    if ev == "HJ_UP":
        if s.hj_consumed:
            # 코드로 이미 소비 -> 아무 동작 없음
            return replace(s, hj_down=False, hj_consumed=False, log=log)
        # 단독 탭: 선택 여부로 한자/기호 결정
        if has_selection and selection_is_hangul:
            log.append("한자후보")
            return replace(s, hj_down=False, candidates_open=True,
                           cand_index=0, log=log)
        log.append("기호팔레트")
        return replace(s, hj_down=False, palette_open=True, log=log)

    # ---------- 방향키 ----------
    if ev in ("LEFT_DOWN", "RIGHT_DOWN"):
        delta = -1 if ev == "LEFT_DOWN" else 1

        # (1) 코드: 한자/기호 키가 눌린 상태 -> 레이아웃 전환
        if s.hj_down:
            new = _cycle(s.layout, delta)
            log.append(f"레이아웃:{new}")
            # 코드 발동 시 열려있던 팝업은 닫는다
            return replace(s, layout=new, hj_consumed=True,
                           candidates_open=False, palette_open=False, log=log)

        # (2) 한자 후보 창이 열려 있으면 후보 탐색
        if s.candidates_open:
            log.append(f"후보이동:{delta:+d}")
            return replace(s, cand_index=s.cand_index + delta, log=log)

        # (3) 기호 팔레트가 열려 있으면 페이지 전환
        if s.palette_open:
            log.append(f"팔레트페이지:{delta:+d}")
            return replace(s, log=log)

        # (4) 평상시: 조합 중이면 확정 후 커서 이동
        log.append("확정+커서이동" if composing else "커서이동")
        return replace(s, log=log)

    if ev == "ESC":
        return replace(s, candidates_open=False, palette_open=False, log=log)

    raise ValueError(ev)


def run(events, **ctx):
    """events: [(ev, kwargs), ...] 또는 [ev, ...]"""
    s = ShellState()
    for e in events:
        if isinstance(e, tuple):
            ev, kw = e
            s = handle(s, ev, **{**ctx, **kw})
        else:
            s = handle(s, e, **ctx)
    return s
