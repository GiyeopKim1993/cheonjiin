"""4x4 16키 배열 검증 — 05-4x4-16키-스펙.md의 모든 수치 출처"""
from engine import (type_keys, CHO, JUNG, JONG, compose, JONG_SPLIT,
                    vowel_seq, State, press)
from encoder import encode, encode_syllable, KEY_OF_JAMO

def conv(seq): return ["KRIGHT" if k=="TIMEOUT" else k for k in seq]
fails=[]

# 1) 음절 내부 멀티탭 충돌 (→ 대체 가능성의 전제조건)
intra=[ch for c in CHO for v in JUNG for j in JONG
       if "TIMEOUT" in encode_syllable(ch:=compose(c,v,j.strip()))[0]]
print(f"음절 내부 멀티탭 충돌: {len(intra)}건")

# 2) 전수 11,172자 (→ 확정 방식)
n=tot=0
for c in CHO:
    for v in JUNG:
        for j in JONG:
            ch=compose(c,v,j.strip()); s=conv(encode(ch))
            if type_keys(s)!=ch: fails.append(("full",ch))
            tot+=len([k for k in s if k!="KRIGHT"]); n+=1
print(f"전수 {n}자 (16키): 실패 {len([f for f in fails if f[0]=='full'])}건")
print(f"평균 타수 (멀티탭 전용): {tot/n:.3f}")

# 3) 롱프레스 쌍자음 도입 시 평균
DBL=set("ㄲㄸㅃㅆㅉ")
def taps(jm,lp): return 1 if (lp and jm in DBL) else KEY_OF_JAMO[jm][1]
def avg(lp):
    t=0
    for c in CHO:
        for v in JUNG:
            for j in JONG:
                js=j.strip(); x=taps(c,lp)+len(vowel_seq(v))
                if js:
                    parts=list(JONG_SPLIT[js]) if js in JONG_SPLIT else [js]
                    x+=sum(taps(p,lp) for p in parts)
                t+=x
    return t/n
print(f"평균 타수 (롱프레스 쌍자음): {avg(True):.3f}")

# 4) 12키 기준선 (기능키 최단)
S={"ㅋ":"ㄱ","ㅌ":"ㄷ","ㅍ":"ㅂ","ㅊ":"ㅈ","ㅎ":"ㅇ"}
D={"ㄲ":"ㄱ","ㄸ":"ㄷ","ㅃ":"ㅂ","ㅆ":"ㅅ","ㅉ":"ㅈ"}
def t12(jm):
    b=KEY_OF_JAMO[jm][1]
    if jm in S: b=min(b,KEY_OF_JAMO[S[jm]][1]+1)
    if jm in D: b=min(b,KEY_OF_JAMO[D[jm]][1]+1)
    return b
t=0
for c in CHO:
    for v in JUNG:
        for j in JONG:
            js=j.strip(); x=t12(c)+len(vowel_seq(v))
            if js:
                parts=list(JONG_SPLIT[js]) if js in JONG_SPLIT else [js]
                x+=sum(t12(p) for p in parts)
            t+=x
print(f"12키 기준선 (기능키 활용): {t/n:.3f}")

# 5) → 확정 동작
for seq,exp in [(["K4","KRIGHT","K4","K1","K2"],"ㄱ가"),(["K4","K4"],"ㅋ"),
                (["K8","K8","KRIGHT","K8"],"ㅎㅅ")]:
    if type_keys(seq)!=exp: fails.append(("commit",seq,type_keys(seq),exp))
# 6) 실전 단어
for w in ["나무위키","한글","안녕하세요","고양이","꽃","빨래","웃음","짜장면",
          "떡볶이","괜찮아","없다","값","닭","삶","밟","읊","오늘 날씨 좋다"]:
    if type_keys(conv(encode(w)))!=w: fails.append(("word",w))
print(f"\n{'✅ 전 항목 통과' if not fails else '❌ 실패: '+str(fails[:10])}")
