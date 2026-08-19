import json, os
os.makedirs("../assets", exist_ok=True)

KEYS = [("ㅣ","1"),("ㆍ","2"),("ㅡ","3"),
        ("ㄱㅋ","4"),("ㄴㄹ","5"),("ㄷㅌ","6"),
        ("ㅂㅍ","7"),("ㅅㅎ","8"),("ㅈㅊ","9"),
        ("획추가","*"),("ㅇㅁ","0"),("쌍자음","#")]

def keypad_svg(path, dark=False):
    bg  = "#101418" if dark else "#F2F4F7"
    key = "#1E252D" if dark else "#FFFFFF"
    fnk = "#2A333D" if dark else "#E4E8EE"
    txt = "#F5F7FA" if dark else "#12171D"
    sub = "#8A94A0"
    acc = "#3B82F6"
    W,H = 400, 300
    kw, kh, gap = 120, 62, 8
    ox, oy = 12, 14
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
             f'<rect width="{W}" height="{H}" rx="14" fill="{bg}"/>']
    for i,(lab,num) in enumerate(KEYS):
        r,c = divmod(i,3)
        x = ox + c*(kw+gap); y = oy + r*(kh+gap)
        isfn = lab in ("획추가","쌍자음")
        fill = fnk if isfn else key
        parts.append(f'<rect x="{x}" y="{y}" width="{kw}" height="{kh}" rx="9" fill="{fill}" '
                     f'stroke="{"#2E3742" if dark else "#D5DAE1"}" stroke-width="1"/>')
        fs = 21 if not isfn else 15
        col = acc if isfn else txt
        parts.append(f'<text x="{x+kw/2}" y="{y+kh/2+7}" font-family="-apple-system,Segoe UI,Roboto,sans-serif" '
                     f'font-size="{fs}" font-weight="600" fill="{col}" text-anchor="middle">{lab}</text>')
        parts.append(f'<text x="{x+kw-9}" y="{y+15}" font-family="sans-serif" font-size="10" '
                     f'fill="{sub}" text-anchor="end">{num}</text>')
    parts.append('</svg>')
    open(path,"w").write("\n".join(parts))

keypad_svg("../assets/keypad-light.svg", False)
keypad_svg("../assets/keypad-dark.svg", True)
print("keypad ok")
