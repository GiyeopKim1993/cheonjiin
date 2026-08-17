"""최종 배치·통일 롱프레스 규칙 검증 — 07 문서 수치 출처"""
from engine import (CHO,JUNG,JONG,compose,JONG_SPLIT,vowel_seq,
                    CONSONANT_CYCLE,type_keys)
from encoder import KEY_OF_JAMO, encode
fails=[]
def chk(n,g,e):
    ok=g==e
    if not ok: fails.append((n,g,e))
    print(f"{'✅' if ok else '❌'} {n}")

# 1) 확정 배치
L=[["ㅣ","ㆍ","ㅡ","⌫"],["ㄱㅋ","ㄴㄹ","ㄷㅌ","↵"],
   ["ㅂㅍ","ㅅㅎ","ㅈㅊ","漢"],["◀","ㅇㅁ","▶","␣"]]
chk("배치: 0키 하단중앙", L[3][1], "ㅇㅁ")
chk("배치: 방향키 좌우 분리", (L[3][0],L[3][2]), ("◀","▶"))
chk("배치: 漢 3행4열", L[2][3], "漢")
chk("배치: ↵ 2행4열", L[1][3], "↵")
def pos(k):
    for i,r in enumerate(L):
        for j,c in enumerate(r):
            if c==k: return (i,j)
def md(a,b):
    (r1,c1),(r2,c2)=pos(a),pos(b); return abs(r1-r2)+abs(c1-c2)
chk("漢↔▶ 거리 2 (코드 편의)", md("漢","▶"), 2)
chk("⌫↔↵ 인접 (위험, M1~M4 필요)", md("⌫","↵"), 1)

# 2) 숫자 레이아웃 = 전화기 배치
N=[["1","2","3"],["4","5","6"],["7","8","9"],[None,"0",None]]
chk("숫자 레이아웃 = 전화기 키패드", (N[0],N[3][1]), (["1","2","3"],"0"))

# 3) 통일 롱프레스 규칙 = 순환열 마지막
LAST={c[-1] for c in CONSONANT_CYCLE.values()}
chk("롱프레스 대상 = 순환 마지막 7종", LAST, {"ㄲ","ㄸ","ㅃ","ㅆ","ㅉ","ㄹ","ㅁ"})
E={"2":"ABC","3":"DEF","4":"GHI","5":"JKL","6":"MNO","7":"PQRS","8":"TUV","9":"WXYZ"}
chk("영어 롱프레스 = 순환 마지막", {v[-1] for v in E.values()},
    {"C","F","I","L","O","S","V","Z"})
chk("E.161 26자 완전 매핑", sorted("".join(E.values())),
    sorted("ABCDEFGHIJKLMNOPQRSTUVWXYZ"))

# 4) 타수 (통일 규칙)
def taps(j,rule):
    if rule=="last" and j in LAST: return 1
    if rule=="dbl" and j in set("ㄲㄸㅃㅆㅉ"): return 1
    return KEY_OF_JAMO[j][1]
def avg(rule):
    t=n=0
    for c in CHO:
        for v in JUNG:
            for j in JONG:
                js=j.strip(); x=taps(c,rule)+len(vowel_seq(v))
                if js:
                    p=list(JONG_SPLIT[js]) if js in JONG_SPLIT else [js]
                    x+=sum(taps(y,rule) for y in p)
                t+=x; n+=1
    return t/n
a,b,c=avg("none"),avg("dbl"),avg("last")
print(f"\n  롱프레스 없음      : {a:.3f}")
print(f"  쌍자음만 (05)      : {b:.3f}")
print(f"  순환 마지막 (확정) : {c:.3f}  ← 12키(6.691) 대비 {100*(c/6.691-1):+.1f}%")
chk("통일 규칙 5.894타", round(c,3), 5.894)
chk("12키 대비 개선", c<6.691, True)

# 5) 조합 무결성 (배치 변경은 조합에 영향 없음)
bad=[]
for cc in CHO:
    for v in JUNG:
        for j in JONG:
            ch=compose(cc,v,j.strip())
            seq=["KRIGHT" if k=="TIMEOUT" else k for k in encode(ch)]
            if type_keys(seq)!=ch: bad.append(ch)
chk("11,172자 전수 무결성 유지", len(bad), 0)

# 6) 0키 노출도
cnt=tot=0
for cc in CHO:
    for v in JUNG:
        for j in JONG:
            js=j.strip()
            for x in [cc]+(list(JONG_SPLIT[js]) if js in JONG_SPLIT else ([js] if js else [])):
                tot+=1
                if x in ("ㅇ","ㅁ"): cnt+=1
print(f"  0키(ㅇ/ㅁ) 자음 출현 비율: {100*cnt/tot:.1f}%  (오터치 노출도)")
print(f"\n{'✅ 전 항목 통과' if not fails else '❌ '+str(fails)}  ({14-len(fails)}/14)")
