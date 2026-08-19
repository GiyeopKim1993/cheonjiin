# 4x4 레이아웃 SVG
KEYS=[  # (main, sub, type)
 ("ㅣ","1","v"),("ㆍ","2","v"),("ㅡ","3","v"),("⌫","","fn"),
 ("ㄱㅋ","4","c"),("ㄴㄹ","5","c"),("ㄷㅌ","6","c"),("한자\n기호","","fn"),
 ("ㅂㅍ","7","c"),("ㅅㅎ","8","c"),("ㅈㅊ","9","c"),("↵","","fn"),
 ("ㅇㅁ","0","c"),("◀","","nav"),("▶","","nav"),("␣","","fn"),
]
def svg(path,dark=False):
    bg="#101418" if dark else "#F2F4F7"
    key="#1E252D" if dark else "#FFFFFF"
    fnk="#2A333D" if dark else "#E4E8EE"
    nav="#243040" if dark else "#DCE6F5"
    txt="#F5F7FA" if dark else "#12171D"
    sub="#8A94A0"; acc="#3B82F6"
    bd="#2E3742" if dark else "#D5DAE1"
    kw,kh,gap=104,60,8; ox,oy=12,12
    W=ox*2+kw*4+gap*3; H=oy*2+kh*4+gap*3
    p=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
       f'<rect width="{W}" height="{H}" rx="14" fill="{bg}"/>']
    for i,(m,s,t) in enumerate(KEYS):
        r,c=divmod(i,4); x=ox+c*(kw+gap); y=oy+r*(kh+gap)
        fill={"fn":fnk,"nav":nav}.get(t,key)
        p.append(f'<rect x="{x}" y="{y}" width="{kw}" height="{kh}" rx="9" fill="{fill}" stroke="{bd}" stroke-width="1"/>')
        col=acc if t in("fn","nav") else txt
        if "\n" in m:
            a,b=m.split("\n")
            p.append(f'<text x="{x+kw/2}" y="{y+kh/2-2}" font-family="sans-serif" font-size="13" font-weight="600" fill="{col}" text-anchor="middle">{a}</text>')
            p.append(f'<text x="{x+kw/2}" y="{y+kh/2+15}" font-family="sans-serif" font-size="13" font-weight="600" fill="{col}" text-anchor="middle">{b}</text>')
        else:
            fs=22 if t in("v","c") else 19
            p.append(f'<text x="{x+kw/2}" y="{y+kh/2+8}" font-family="sans-serif" font-size="{fs}" font-weight="600" fill="{col}" text-anchor="middle">{m}</text>')
        if s:
            p.append(f'<text x="{x+kw-8}" y="{y+15}" font-family="sans-serif" font-size="10" fill="{sub}" text-anchor="end">{s}</text>')
    p.append('</svg>')
    open(path,"w").write("\n".join(p))
svg("../assets/keypad16-light.svg",False)
svg("../assets/keypad16-dark.svg",True)
print("ok")
