#!/usr/bin/env python3
"""
한국어 위키낱말사전 덤프에서 한자어(표제어 -> 한자 표기)를 추출한다.

소스: https://dumps.wikimedia.org/kowiktionary/latest/kowiktionary-latest-pages-articles.xml.bz2
  (API 레이트 리밋 없음 / 재현 가능 / 1회 다운로드)

채택 규칙 — 오매칭을 막는 3중 필터:
  1. 표제어는 순한글, 한자 표기는 KS X 1001 한자로만 구성  → 일본 신자체(経済) 자동 배제
  2. 표제어 음절 수 == 한자 수
  3. 각 한자의 Unihan kHangul 음이 대응 음절과 일치        → 우연한 문자열 매칭 차단

동음이의어 처리 — **후보를 하나로 줄이지 않는다**:
  위키낱말사전은 한 문서에 여러 어원을 담는다(전화 = 田禾/典貨/電火/電化/電話...).
  이전 버전은 best 하나만 저장해서 '가정'에 假定만 남고 家庭이 사라졌다.
  이제 유효 후보를 **전부** 보존하고 상용도 순으로 정렬해 배열로 저장한다.

  정렬 기준 (앞일수록 먼저 제시):
    1. 문서 내 등장 횟수가 많은 것   — 주표기일 가능성이 높다
    2. 구성 한자의 상용도 합이 낮은 것 — 각 글자가 음절 목록 앞쪽일수록 상용

  후보 상한(MAX_CANDS)을 둬 희귀 표기가 목록을 오염시키지 않게 한다.

사용:
  python3 tools/fetch_words.py [덤프경로]     # 기본 /tmp/kowikt.xml.bz2
출력:
  spec/words-raw.json
"""
import bz2, json, os, re, sys, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DUMP = sys.argv[1] if len(sys.argv) > 1 else "/tmp/kowikt.xml.bz2"
DUMP_URL = ("https://dumps.wikimedia.org/kowiktionary/latest/"
            "kowiktionary-latest-pages-articles.xml.bz2")


# 일본 신자체/중국 간체 -> KS X 1001 정자 (위키가 정자를 싣지 않은 경우 폴백)
SIMPLIFIED = {
    "教":"敎","経":"經","済":"濟","学":"學","国":"國","会":"會","医":"醫","数":"數",
    "験":"驗","覚":"覺","労":"勞","権":"權","当":"當","対":"對","浜":"濱","歳":"歲",
    "帰":"歸","県":"縣","戦":"戰","拡":"擴","晩":"晚","気":"氣","楽":"樂","歩":"步",
    "産":"產","発":"發","県":"縣","真":"眞","研":"硏","社":"社","者":"者","収":"收",
    "総":"總","増":"增","蔵":"藏","装":"裝","読":"讀","変":"變","弁":"辨","仏":"佛",
    "写":"寫","処":"處","将":"將","状":"狀","乗":"乘","畳":"疊","縄":"繩","静":"靜",
    "隠":"隱","栄":"榮","営":"營","衛":"衞","駅":"驛","円":"圓","塩":"鹽","応":"應",
    "横":"橫","温":"溫","仮":"假","価":"價","絵":"繪","閣":"閣","覧":"覽","歴":"歷",
    "練":"練","恋":"戀","霊":"靈","齢":"齡","炉":"爐","湾":"灣","満":"滿","黙":"默",
    "訳":"譯","薬":"藥","様":"樣","与":"與","余":"餘","誉":"譽","予":"豫","landmark":"",
}


# 한 표제어에 남길 최대 후보 수. 위키는 벽자까지 싣기 때문에 상한이 없으면
# '전화' 같은 항목에 田禾/典貨 같은 고어가 줄줄이 붙어 목록을 버린다.
MAX_CANDS = 6


def ks_x_1001_hanja():
    """EUC-KR 코덱으로 KS X 1001 한자 영역을 직접 열거 (외부 데이터 불필요)"""
    out = set()
    for hi in range(0xCA, 0xFE):
        for lo in range(0xA1, 0xFF):
            try: out.add(bytes([hi, lo]).decode("euc-kr"))
            except Exception: pass
    return out


def iter_pages(path):
    """<title> / <text> 쌍을 스트리밍으로 뽑는다 (전체 XML 파싱 없이)"""
    title_re = re.compile(rb"<title>(.*?)</title>", re.S)
    text_re  = re.compile(rb"<text[^>]*>(.*?)</text>", re.S)
    buf = b""
    with bz2.open(path, "rb") as f:
        while True:
            chunk = f.read(1 << 20)
            if not chunk:
                break
            buf += chunk
            while True:
                i = buf.find(b"</page>")
                if i < 0: break
                page, buf = buf[:i], buf[i + 7:]
                t = title_re.search(page)
                x = text_re.search(page)
                if t and x:
                    yield (t.group(1).decode("utf-8", "replace"),
                           x.group(1).decode("utf-8", "replace"))
            if len(buf) > (1 << 24):        # 안전장치
                buf = buf[-(1 << 20):]


def main():
    if not os.path.exists(DUMP):
        sys.exit(f"덤프 없음: {DUMP}\n  curl -o {DUMP} {DUMP_URL}")

    ks = ks_x_1001_hanja()
    dic = json.load(open(f"{ROOT}/spec/hanja-dict.json"))
    reading = collections.defaultdict(set)          # 한자 -> {음}
    for syl, chars in dic["chars"].items():
        for c in chars:
            reading[c].add(syl)

    # 글자별 상용 순위: hanja-dict.json 의 음절 배열 인덱스 (앞일수록 상용)
    rank = {}
    for syl, chars in dic["chars"].items():
        for i, c in enumerate(chars):
            rank[(c, syl)] = i

    hangul_title = re.compile(r"^[가-힣]+$")
    hanja_run = re.compile(r"[\u4E00-\u9FFF\uF900-\uFAFF]+")

    words = {}
    stats = collections.Counter()
    seen = 0

    for title, body in iter_pages(DUMP):
        seen += 1
        if seen % 100000 == 0:
            sys.stderr.write(f"\r  스캔 {seen:,} / 채택 {len(words):,}")
            sys.stderr.flush()
        if ":" in title or not hangul_title.match(title):
            continue
        # 한국어 섹션만 (== {{한국어}} == 또는 =={{ko}}== 등)
        ko = body
        m = re.search(r"==\s*\{\{?\s*(한국어|ko)\s*\}?\}?\s*==", body)
        if m:
            nxt = re.search(r"\n==[^=]", body[m.end():])
            ko = body[m.end(): m.end() + nxt.start()] if nxt else body[m.end():]
        n = len(title)
        stats["hangulTitle"] += 1

        # ── 어원 섹션별 "설명 분량"을 재본다 ──
        # 위키낱말사전은 동음이의어를 "*어원: 한자 [[假定]]" 로 구분해 나열한다.
        # 각 어원 섹션의 위키링크 수(유의어/합성어/파생어/번역)가 실사용 빈도를
        # 가장 잘 반영한다. 실측: 家庭 16 vs 假定 7, 詐欺 53 vs 史記 4,
        # 首都 90, 記事 56 — 사람이 기대하는 1순위와 일치했다.
        # 문서 내 단순 등장 횟수는 신호가 약해(家庭 0회) 이걸로 대체한다.
        etym_weight = {}
        marks = [(mm.start(), mm.group(1))
                 for mm in re.finditer(r"어원:\s*한자\s*\[\[([\u4E00-\u9FFF]+)\]\]", ko)]
        for i, (pos, hj) in enumerate(marks):
            end = marks[i + 1][0] if i + 1 < len(marks) else len(ko)
            seg = ko[pos:end]
            # 링크 수를 주 신호로, 분량을 보조로.
            # 같은 한자가 여러 어원 섹션에 나뉘어 나오면 **합산**한다.
            # (max 로 했더니 '가계'처럼 家計가 3개 섹션에 쪼개진 항목에서
            #  각 섹션이 작아 家系 에 밀렸다 — 등장 자체가 빈도 신호다)
            wgt = seg.count("[[") * 10 + len(seg) // 100
            etym_weight[hj] = etym_weight.get(hj, 0) + wgt

        # 모든 유효 후보를 수집 (동음이의어 대비)
        cands = []
        for raw in hanja_run.findall(ko):
            if len(raw) != n:
                continue
            # 신자체가 섞였으면 정자로 변환 시도 (교육: 教育 -> 敎育)
            cand = "".join(SIMPLIFIED.get(ch, ch) for ch in raw)
            if cand != raw:
                stats["simplifiedFixed"] += 1
            if not all(ch in ks for ch in cand):
                stats["nonKS"] += 1
                continue
            if not all(syl in reading.get(ch, ()) for ch, syl in zip(cand, title)):
                stats["readingMismatch"] += 1
                continue
            cands.append(cand)

        if not cands:
            stats["noMatch"] += 1
            continue

        # 문서 내 등장 횟수(많을수록 주표기) + 구성 한자 상용도로 순위
        # ── 순위 결정 ──
        # 세 신호를 각각 0~1 로 정규화해 가중합한다. 절대값끼리 비교하면
        # 문서마다 스케일이 달라(어원가중 15~860) 한 신호가 나머지를 압도한다.
        #
        # 가중치 (1, 2, 1) 은 눈대중이 아니라 정답 세트로 정한 값이다:
        #   spec/word-rank-truth.tsv (사람이 고른 1순위 50개)
        #   격자탐색 최적 46/49(94%) · 5-fold 교차검증 평균 89.8%
        #   -> 과적합이 아니며 5개 fold 중 3개에서 같은 값이 뽑혔다.
        # 재현/재조정: python3 tools/tune_word_rank.py
        W_ETYM, W_FREQ, W_USAGE = 1.0, 2.0, 1.0

        freq = collections.Counter(cands)
        uniq = sorted(set(cands))
        mx_e = max((etym_weight.get(x, 0) for x in uniq), default=0) or 1
        mx_f = max(freq[x] for x in uniq) or 1
        usage_of = {x: sum(rank.get((ch, syl), 99) for ch, syl in zip(x, title))
                    for x in uniq}
        mx_u = max(usage_of.values()) or 1

        def score(c):
            v = (W_ETYM  * (etym_weight.get(c, 0) / mx_e)
               + W_FREQ  * (freq[c] / mx_f)
               - W_USAGE * (usage_of[c] / mx_u))
            # 점수 내림차순, 동점이면 상용도가 낮은(=흔한) 쪽 먼저
            return (-v, usage_of[c])
        ranked = sorted(set(cands), key=score)[:MAX_CANDS]
        words[title] = ranked
        if len(ranked) > 1:
            stats["homonym"] += 1
            stats["extraCands"] += len(ranked) - 1

    sys.stderr.write("\n")
    print(f"문서 {seen:,} 스캔 / 한글 표제어 {stats['hangulTitle']:,}")
    print(f"채택 {len(words):,}개")
    print(f"배제: 비KS {stats['nonKS']:,} / 음불일치 {stats['readingMismatch']:,} / 매칭없음 {stats['noMatch']:,}")
    print(f"동음이의 해소: {stats['homonym']:,}건 / 신자체 교정: {stats['simplifiedFixed']:,}건")

    out = f"{ROOT}/spec/words-raw.json"
    json.dump(words, open(out, "w"), ensure_ascii=False, indent=0, sort_keys=True)
    print(f"저장: {out}")
    for k in list(sorted(words))[:12]:
        print(f"  {k} -> {words[k]}")


if __name__ == "__main__":
    main()
