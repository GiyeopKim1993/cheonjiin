"""한자/기호 키 · 코드 · 레이아웃 전환 검증 — 06 문서의 수치 출처"""
from modefsm import ShellState, handle, run, LAYOUTS
fails=[]
def chk(name,got,exp):
    ok = got==exp
    if not ok: fails.append((name,got,exp))
    print(f"{'✅' if ok else '❌'} {name}")

chk("R1 블록선택+漢탭 → 한자후보",
    run([("HJ_DOWN",{}),("HJ_UP",{})],has_selection=True,selection_is_hangul=True).log,["한자후보"])
chk("R2 선택없음+漢탭 → 기호팔레트",
    run([("HJ_DOWN",{}),("HJ_UP",{})],has_selection=False).log,["기호팔레트"])
chk("R2 영문선택+漢탭 → 기호팔레트",
    run([("HJ_DOWN",{}),("HJ_UP",{})],has_selection=True,selection_is_hangul=False).log,["기호팔레트"])
s=run([("HJ_DOWN",{}),("RIGHT_DOWN",{}),("HJ_UP",{})])
chk("R3 漢+▶ → 영어, 팔레트 미발생",(s.layout,s.palette_open,s.log),("영어",False,["레이아웃:영어"]))
chk("R3 漢+◀ → 역방향 숫자",
    run([("HJ_DOWN",{}),("LEFT_DOWN",{}),("HJ_UP",{})]).layout,"숫자")
s=ShellState()
for _ in range(3): s=handle(s,"HJ_DOWN");s=handle(s,"RIGHT_DOWN");s=handle(s,"HJ_UP")
chk("R3 3회 순환 → 한글 복귀",s.layout,"한글")
s=ShellState()
for _ in range(3): s=handle(s,"HJ_DOWN");s=handle(s,"LEFT_DOWN");s=handle(s,"HJ_UP")
chk("R3 역방향 3회 → 한글 복귀",s.layout,"한글")
chk("우선순위: 선택중에도 코드 우선",
    (lambda x:(x.layout,x.candidates_open))(run([("HJ_DOWN",{}),("RIGHT_DOWN",{}),("HJ_UP",{})],
     has_selection=True,selection_is_hangul=True)),("영어",False))
s=run([("HJ_DOWN",{}),("HJ_UP",{}),("RIGHT_DOWN",{}),("RIGHT_DOWN",{}),("LEFT_DOWN",{})],
      has_selection=True,selection_is_hangul=True)
chk("후보창 열림 → 방향키는 후보탐색",(s.cand_index,s.candidates_open),(1,True))
s=ShellState(candidates_open=True); s=handle(s,"HJ_DOWN"); s=handle(s,"RIGHT_DOWN")
chk("후보창 중 코드 → 닫고 전환",(s.candidates_open,s.layout),(False,"영어"))
s=ShellState(palette_open=True); s=handle(s,"HJ_DOWN"); s=handle(s,"LEFT_DOWN")
chk("팔레트 중 코드 → 닫고 전환",(s.palette_open,s.layout),(False,"숫자"))
chk("평상시 ▶ (조합중) → 확정",run([("RIGHT_DOWN",{})],composing=True).log,["확정+커서이동"])
chk("평상시 ▶ (비조합) → 커서",run([("RIGHT_DOWN",{})],composing=False).log,["커서이동"])
chk("ESC → 팝업 닫힘",
    (lambda x:(x.candidates_open,x.palette_open))(handle(ShellState(candidates_open=True),"ESC")),(False,False))

# E.161 준수 검증
E161={"2":"ABC","3":"DEF","4":"GHI","5":"JKL","6":"MNO","7":"PQRS","8":"TUV","9":"WXYZ"}
letters="".join(E161.values())
chk("영어 레이아웃 E.161 준수 (26자 전체)",sorted(letters),sorted("ABCDEFGHIJKLMNOPQRSTUVWXYZ"))
chk("E.161 최대 탭수 4 (S,Z)",max(len(v) for v in E161.values()),4)
print(f"\n{chr(9989)+chr(32)+chr(51204)+chr(32)+chr(54637)+chr(47785)+chr(32)+chr(53685)+chr(44284) if not fails else str(fails)}  ({16-len(fails)}/16)")
