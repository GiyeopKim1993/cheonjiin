from engine import V, PENDING, VOWEL_KEYS
from collections import deque

# BFS 트리 레이아웃 (깊이 = 타수)
inv = {j:k for k,j in VOWEL_KEYS.items()}
NUM = {"ㅣ":"1","ㆍ":"2","ㅡ":"3"}
depth={"":0}; parent={}; order=[]
q=deque([""])
while q:
    st=q.popleft(); order.append(st)
    for jamo,nxt in V.get(st,{}).items():
        if nxt and nxt not in depth:
            depth[nxt]=depth[st]+1; parent[nxt]=(st,jamo); q.append(nxt)

bylv={}
for s in order:
    if s=="" : continue
    bylv.setdefault(depth[s],[]).append(s)

COLW, ROWH = 150, 46
maxlv=max(bylv); maxrow=max(len(v) for v in bylv.values())
W = 90 + COLW*maxlv + 60
H = 60 + ROWH*maxrow
pos={"":(46, H/2)}
for lv,items in bylv.items():
    items.sort()
    total=len(items)
    for i,s in enumerate(items):
        y = H/2 - (total-1)*ROWH/2 + i*ROWH
        pos[s]=(46+COLW*lv, y)

p=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
   f'<rect width="{W}" height="{H}" fill="#FBFCFD"/>',
   '<style>.n{font:600 15px sans-serif}.e{font:10px sans-serif;fill:#6B7684}.h{font:600 11px sans-serif;fill:#8A94A0}</style>']
for lv in range(1,maxlv+1):
    p.append(f'<text x="{46+COLW*lv}" y="18" class="h" text-anchor="middle">{lv}타</text>')
# edges
for s,(par,jamo) in parent.items():
    x1,y1=pos[par]; x2,y2=pos[s]
    mx=(x1+x2)/2
    p.append(f'<path d="M{x1+22} {y1} C{mx} {y1}, {mx} {y2}, {x2-22} {y2}" fill="none" stroke="#C9D0D8" stroke-width="1.2"/>')
    p.append(f'<text x="{mx}" y="{(y1+y2)/2-3}" class="e" text-anchor="middle">{NUM[jamo]}</text>')
# nodes
for s,(x,y) in pos.items():
    if s=="":
        p.append(f'<circle cx="{x}" cy="{y}" r="16" fill="#12171D"/>')
        p.append(f'<text x="{x}" y="{y+5}" class="n" fill="#fff" text-anchor="middle">시작</text>')
        continue
    pend = s in PENDING
    fill = "#FFF3D6" if pend else "#E8F0FE"
    stroke = "#E0A800" if pend else "#3B82F6"
    p.append(f'<rect x="{x-21}" y="{y-15}" width="42" height="30" rx="8" fill="{fill}" stroke="{stroke}" stroke-width="1.4"'
             + (' stroke-dasharray="3 2"' if pend else '') + '/>')
    p.append(f'<text x="{x}" y="{y+6}" class="n" fill="#12171D" text-anchor="middle">{s}</text>')
p.append(f'<text x="14" y="{H-12}" class="e">실선=확정 모음 · 점선(노랑)=미완성 중간상태(화면 미출력) · 간선 숫자=키</text>')
p.append('</svg>')
open("../assets/vowel-automaton.svg","w").write("\n".join(p))
print("ok", W, H, "states:", len(pos)-1)
