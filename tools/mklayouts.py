# 한글/영어/숫자 3개 레이아웃 SVG (4x4)
LAYOUTS = {
 "hangul": [("ㅣ","1","v"),("ㆍ","2","v"),("ㅡ","3","v"),("⌫","","fn"),
            ("ㄱㅋ","4","c"),("ㄴㄹ","5","c"),("ㄷㅌ","6","c"),("↵","","fn"),
            ("ㅂㅍ","7","c"),("ㅅㅎ","8","c"),("ㅈㅊ","9","c"),("漢","","hj"),
            ("◀","","nav"),("ㅇㅁ","0","c"),("▶","","nav"),("␣","","fn")],
 "english":[(".,?","1","c"),("ABC","2","c"),("DEF","3","c"),("⌫","","fn"),
            ("GHI","4","c"),("JKL","5","c"),("MNO","6","c"),("↵","","fn"),
            ("PQRS","7","c"),("TUV","8","c"),("WXYZ","9","c"),("漢","","hj"),
            ("◀","","nav"),("⇧","0","c"),("▶","","nav"),("␣","","fn")],
 "number": [("1","","c"),("2","","c"),("3","","c"),("⌫","","fn"),
            ("4","","c"),("5","","c"),("6","","c"),("↵","","fn"),
            ("7","","c"),("8","","c"),("9","","c"),("漢","","hj"),
            ("◀","","nav"),("0","","c"),("▶","","nav"),("␣","","fn")],
}
TITLE={"hangul":"한글","english":"영어","number":"숫자"}
def svg(name,keys,dark=False):
    bg="#101418" if dark else "#F2F4F7"; key="#1E252D" if dark else "#FFFFFF"
    fnk="#2A333D" if dark else "#E4E8EE"; nav="#243040" if dark else "#DCE6F5"
    hjc="#3A2E1A" if dark else "#FDF0D5"
    txt="#F5F7FA" if dark else "#12171D"; sub="#8A94A0"; acc="#3B82F6"
    hja="#B4791F" if not dark else "#E0A800"
    bd="#2E3742" if dark else "#D5DAE1"
    kw,kh,gap=104,60,8; ox,oy=12,40
    W=ox*2+kw*4+gap*3; H=oy+12+kh*4+gap*3
    p=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
       f'<rect width="{W}" height="{H}" rx="14" fill="{bg}"/>',
       f'<text x="{ox}" y="26" font-family="sans-serif" font-size="14" font-weight="700" fill="{txt}">{TITLE[name]}</text>',
       f'<text x="{W-ox}" y="26" font-family="sans-serif" font-size="11" fill="{sub}" text-anchor="end">漢 + ◀▶ 로 전환</text>']
    for i,(m,s,t) in enumerate(keys):
        r,c=divmod(i,4); x=ox+c*(kw+gap); y=oy+r*(kh+gap)
        fill={"fn":fnk,"nav":nav,"hj":hjc}.get(t,key)
        p.append(f'<rect x="{x}" y="{y}" width="{kw}" height="{kh}" rx="9" fill="{fill}" stroke="{bd}" stroke-width="1"/>')
        col=hja if t=="hj" else (acc if t in("fn","nav") else txt)
        fs=22 if t in("v","c") else 19
        if t=="c" and len(m)>=3: fs=18 if len(m)==3 else 16
        p.append(f'<text x="{x+kw/2}" y="{y+kh/2+8}" font-family="sans-serif" font-size="{fs}" font-weight="600" fill="{col}" text-anchor="middle">{m}</text>')
        if s: p.append(f'<text x="{x+kw-8}" y="{y+15}" font-family="sans-serif" font-size="10" fill="{sub}" text-anchor="end">{s}</text>')
    p.append('</svg>')
    open(f"../assets/layout-{name}.svg","w").write("\n".join(p))
for n,k in LAYOUTS.items(): svg(n,k)
print("ok")
