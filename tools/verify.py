"""엔진 검증 + 스펙 문서용 표/통계 자동 생성 (모든 수치의 출처)"""
import json
from engine import (State, press, type_keys, vowel_seq, JUNG, CHO, JONG,
                    CONSONANT_CYCLE, V, compose, JONG_SPLIT)
from encoder import encode, KEY_OF_JAMO, decompose

LABEL = {"K1": "1", "K2": "2", "K3": "3", "K4": "4", "K5": "5", "K6": "6",
         "K7": "7", "K8": "8", "K9": "9", "KSTAR": "*", "K0": "0", "KHASH": "#",
         "SPACE": "␣", "BACK": "⌫", "TIMEOUT": "·"}
def fmt(seq): return "".join(LABEL[k] for k in seq)

report = {}
fails = []

# ---------- 1) 중성 21자 전수
vowel_rows = []
for v in JUNG:
    seq = vowel_seq(v)
    out = type_keys(["K0"] + seq)
    ok = out == compose("ㅇ", v)
    if not ok: fails.append(("vowel", v, out))
    vowel_rows.append({"jamo": v, "keys": fmt(seq), "taps": len(seq), "render": out})
report["vowel"] = vowel_rows

# ---------- 2) 초성 19자 전수 (멀티탭)
cho_rows = []
for c in CHO:
    k, taps = KEY_OF_JAMO[c]
    seq = [k] * taps
    out = type_keys(seq + ["K1", "K2"])
    ok = out == compose(c, "ㅏ")
    if not ok: fails.append(("cho", c, out))
    cho_rows.append({"jamo": c, "keys": fmt(seq), "taps": taps, "render": out})
report["cho"] = cho_rows

# ---------- 3) 획추가/쌍자음 대체 경로 (기능키 경로)
alt_rows = []
ALT = {"ㅋ": ["K4","KSTAR"], "ㄲ": ["K4","KHASH"], "ㅌ": ["K6","KSTAR"], "ㄸ": ["K6","KHASH"],
       "ㅍ": ["K7","KSTAR"], "ㅃ": ["K7","KHASH"], "ㅎ": ["K0","KSTAR"], "ㅆ": ["K8","KHASH"],
       "ㅊ": ["K9","KSTAR"], "ㅉ": ["K9","KHASH"], "ㄷ": ["K5","KSTAR"], "ㅈ": ["K8","KSTAR"]}
for c, seq in ALT.items():
    out = type_keys(seq + ["K1", "K2"])
    ok = out == compose(c, "ㅏ")
    if not ok: fails.append(("alt", c, out))
    alt_rows.append({"jamo": c, "keys": fmt(seq), "taps": len(seq), "render": out, "ok": ok})
report["alt"] = alt_rows

# ---------- 4) 겹받침 11종 전수
jong_rows = []
for cluster in JONG_SPLIT:
    ch = compose("ㄱ", "ㅏ", cluster)
    seq = encode(ch)
    out = type_keys(seq)
    ok = out == ch
    if not ok: fails.append(("jong", ch, out))
    jong_rows.append({"cluster": cluster, "sample": ch, "keys": fmt(seq),
                      "taps": len([k for k in seq if k != "TIMEOUT"]), "render": out})
report["jong"] = jong_rows

# ---------- 5) 실전 단어/문장 (인코더 생성 → 엔진 왕복)
WORDS = ["나무위키", "한글", "안녕하세요", "고양이", "꽃", "빨래", "웃음",
         "짜장면", "떡볶이", "괜찮아", "값", "닭", "삶", "밟", "읊", "없다",
         "오늘 날씨 좋다", "회의 일정 확인 부탁드립니다"]
word_rows = []
for w in WORDS:
    seq = encode(w)
    out = type_keys(seq)
    ok = out == w
    if not ok: fails.append(("word", w, out))
    n = len([k for k in seq if k != "TIMEOUT"])
    word_rows.append({"word": w, "keys": fmt(seq), "taps": n,
                      "per_char": round(n / len(w.replace(" ", "")), 2), "render": out})
report["word"] = word_rows

# ---------- 6) 백스페이스 자모 역순 삭제
bs_rows = []
BS = [(["K4","K1","K2","K5"], "가"),          # 각 -> 가
      (["K4","K2","K3","K1"], "고"),          # 괴 -> 고
      (["K4","K1","K2","K4","TIMEOUT","K8"], "각"),  # 갃 -> 각
      (["K4","K4"], ""),                       # ㅋ -> (빈)
      (["K5","K1","K2"], "니")]                # 나 -> 니 (ㆍ만 제거)
for seq, expect in BS:
    s = State()
    for k in seq: s = press(s, k)
    s = press(s, "BACK")
    out = press(s, "COMMIT").committed
    ok = out == expect
    if not ok: fails.append(("bs", fmt(seq), out, expect))
    bs_rows.append({"keys": fmt(seq) + "⌫", "result": out, "expect": expect, "ok": ok})
report["bs"] = bs_rows

# ---------- 7) 11,172자 전수 왕복 + 타수 분포
total = taps = 0
dist = {}
for c in CHO:
    for v in JUNG:
        for j in JONG:
            ch = compose(c, v, j.strip())
            seq = encode(ch)
            n = len([k for k in seq if k != "TIMEOUT"])
            if type_keys(seq) != ch: fails.append(("full", ch))
            total += 1; taps += n
            dist[n] = dist.get(n, 0) + 1
report["full"] = {"total": total, "avg_taps": round(taps / total, 3),
                  "dist": dict(sorted(dist.items()))}

# ---------- 8) 두벌식 대비 타수 (두벌식 = 자모 1개당 1타, 쌍자음/ㅐㅔ류는 shift 포함)
DUBEOL_SHIFT = set("ㄲㄸㅃㅆㅉㅒㅖ")
def dubeol_taps(text):
    t = 0
    for ch in text:
        if ch == " ": t += 1; continue
        cho, jung, jong = decompose(ch)
        for jamo in (cho, jung, jong):
            if not jamo: continue
            if jamo in JONG_SPLIT:            # 겹받침 = 2타
                a, b = JONG_SPLIT[jamo]; t += 2
                continue
            # 복합모음은 두벌식에서도 2타 (ㅘ=ㅗ+ㅏ 등)
            COMPLEX = {"ㅘ":2,"ㅙ":3,"ㅚ":2,"ㅝ":2,"ㅞ":3,"ㅟ":2,"ㅢ":2}
            t += COMPLEX.get(jamo, 1)
            if jamo in DUBEOL_SHIFT: t += 1
    return t

CORPUS = ["오늘 회의 자료 공유드립니다", "내일 아침 아홉시까지 사무실로 와주세요",
          "감사합니다 좋은 하루 되세요", "지하철 막차 시간 확인했어요",
          "우리 같이 밥 먹을래", "괜찮으면 전화 주세요"]
cmp_rows = []
for line in CORPUS:
    cj = len([k for k in encode(line) if k != "TIMEOUT"])
    db = dubeol_taps(line)
    cmp_rows.append({"text": line, "cheonjiin": cj, "dubeolsik": db,
                     "ratio": round(cj / db, 2)})
tot_cj = sum(r["cheonjiin"] for r in cmp_rows)
tot_db = sum(r["dubeolsik"] for r in cmp_rows)
report["compare"] = {"rows": cmp_rows, "total_cheonjiin": tot_cj,
                     "total_dubeolsik": tot_db, "ratio": round(tot_cj / tot_db, 3)}

# ---------- 9) 멀티탭 충돌 빈도 (TIMEOUT 필요 비율)
to_needed = sum(1 for line in CORPUS for k in encode(line) if k == "TIMEOUT")
to_total = sum(len(encode(line)) for line in CORPUS)
report["timeout"] = {"count": to_needed, "keys": to_total,
                     "pct": round(100 * to_needed / to_total, 2)}

print(f"전수 {total}자 / 실패 {len(fails)}건 / 평균 {report['full']['avg_taps']} 타")
print(f"타수 분포: {report['full']['dist']}")
print(f"천지인 {tot_cj}타 vs 두벌식 {tot_db}타 = {report['compare']['ratio']}배")
print(f"멀티탭 충돌(TIMEOUT) 비율: {report['timeout']['pct']}%")
print(f"중성 평균 {sum(r['taps'] for r in vowel_rows)/21:.2f}타, "
      f"초성 평균 {sum(r['taps'] for r in cho_rows)/19:.2f}타")
if fails:
    print("\n=== FAILS ===")
    for f in fails[:20]: print(f)
else:
    print("\n✅ 전 항목 통과")

json.dump(report, open("results.json", "w"), ensure_ascii=False, indent=1)
