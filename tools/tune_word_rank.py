#!/usr/bin/env python3
"""동음이의어 1순위 정렬 가중치 튜닝 + 검증

fetch_words.py 의 W_ETYM / W_FREQ / W_USAGE 를 **눈대중으로 정하지 않기 위한**
도구다. 정답 세트(spec/word-rank-truth.tsv)에 대해 격자탐색과 5-fold 교차검증을
수행한다.

왜 필요한가:
  '가정'에 家庭 대신 假定이 먼저 뜨면 사용자가 매번 후보를 넘겨야 한다.
  파라미터를 손으로 만지면 특정 예시만 맞고 다른 게 깨진다(실제로 겪음:
  band=30 으로 맞췄더니 '전화'가 電火로 퇴행).

신호 3종:
  etym  — 어원 섹션의 위키링크 수. 유의어/합성어/번역이 많을수록 주표기.
  freq  — 문서 내 등장 횟수.
  usage — 구성 한자의 상용도 합(작을수록 흔한 글자). **낮을수록 좋다**.

사용법:
  python3 tools/tune_word_rank.py [덤프경로]     # 기본 /tmp/kowikt.xml.bz2
  python3 tools/tune_word_rank.py --check        # 현재 가중치만 검증(캐시 사용)
"""
import bz2, json, os, re, sys, collections, pickle, random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TRUTH = os.path.join(ROOT, 'spec', 'word-rank-truth.tsv')
CACHE = '/tmp/word_rank_cache.pkl'

# fetch_words.py 와 동일해야 하는 현재 운용 가중치
CURRENT = (1.0, 2.0, 1.0)


def load_truth():
    t = {}
    with open(TRUTH, encoding='utf-8') as f:
        for ln in f:
            ln = ln.strip()
            if not ln or ln.startswith('#'):
                continue
            k, v = ln.split('\t')
            t[k] = v
    return t


def ks_x_1001_hanja():
    out = set()
    for hi in range(0xCA, 0xFE):
        for lo in range(0xA1, 0xFF):
            try:
                out.add(bytes([hi, lo]).decode('euc-kr'))
            except Exception:
                pass
    return out


def build_cache(dump, truth):
    """정답 세트 표제어에 대해서만 후보와 신호를 추출해 캐시한다."""
    ks = ks_x_1001_hanja()
    dic = json.load(open(os.path.join(ROOT, 'spec', 'hanja-dict.json'),
                         encoding='utf-8'))
    chars = dic['chars']
    rank, reading = {}, collections.defaultdict(set)
    for syl, lst in chars.items():
        for i, ch in enumerate(lst):
            rank[(ch, syl)] = i
            reading[ch].add(syl)

    hanja_run = re.compile(r'[\u4E00-\u9FFF]+')
    cache = {}
    title, buf, intext = None, [], False
    with bz2.open(dump, 'rt', encoding='utf-8') as f:
        for line in f:
            if '<title>' in line:
                title = line.split('<title>')[1].split('</title>')[0]
            elif '<text' in line:
                intext, buf = True, [line.split('>', 1)[-1]]
            elif intext:
                if '</text>' in line:
                    intext = False
                    if title in truth:
                        body = ''.join(buf) + line.split('</text>')[0]
                        m = re.search(r"==\s*\{\{?\s*(한국어|ko)\s*\}?\}?\s*==", body)
                        ko = body
                        if m:
                            nxt = re.search(r"\n==[^=]", body[m.end():])
                            ko = (body[m.end(): m.end() + nxt.start()]
                                  if nxt else body[m.end():])
                        n = len(title)
                        cands = []
                        for raw in hanja_run.findall(ko):
                            if len(raw) != n:
                                continue
                            if not all(c in ks for c in raw):
                                continue
                            if not all(s in reading.get(c, ())
                                       for c, s in zip(raw, title)):
                                continue
                            cands.append(raw)
                        marks = [(mm.start(), mm.group(1)) for mm in re.finditer(
                            r"어원:\s*한자\s*\[\[([\u4E00-\u9FFF]+)\]\]", ko)]
                        ew = collections.Counter()
                        for i, (pos, hj) in enumerate(marks):
                            end = marks[i + 1][0] if i + 1 < len(marks) else len(ko)
                            seg = ko[pos:end]
                            ew[hj] += seg.count('[[') * 10 + len(seg) // 100
                        if cands:
                            cache[title] = (cands, dict(ew))
                    buf = []
                else:
                    buf.append(line)
    pickle.dump((cache, rank), open(CACHE, 'wb'))
    return cache, rank


def accuracy(params, subset, cache, rank, truth, want_miss=False):
    a, b, c = params
    ok, miss = 0, []
    for title in subset:
        cands, ew = cache[title]
        uniq = sorted(set(cands))
        freq = collections.Counter(cands)
        mx_e = max((ew.get(x, 0) for x in uniq), default=0) or 1
        mx_f = max(freq[x] for x in uniq) or 1
        us = {x: sum(rank.get((ch, syl), 99) for ch, syl in zip(x, title))
              for x in uniq}
        mx_u = max(us.values()) or 1

        def sc(x):
            return (a * (ew.get(x, 0) / mx_e) + b * (freq[x] / mx_f)
                    - c * (us[x] / mx_u))

        best = max(uniq, key=lambda x: (sc(x), -us[x]))
        if best == truth[title]:
            ok += 1
        else:
            miss.append((title, best, truth[title]))
    return (ok / len(subset), miss) if want_miss else ok / len(subset)


def main():
    truth = load_truth()
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    dump = args[0] if args else '/tmp/kowikt.xml.bz2'

    if '--check' in sys.argv and os.path.exists(CACHE):
        cache, rank = pickle.load(open(CACHE, 'rb'))
    else:
        if not os.path.exists(dump):
            raise SystemExit(f"❌ 덤프 없음: {dump}")
        print(f"덤프 스캔 중… ({os.path.basename(dump)})")
        cache, rank = build_cache(dump, truth)

    keys = sorted(cache)
    print(f"정답 {len(truth)}개 / 캐시 {len(keys)}개")

    acc, miss = accuracy(CURRENT, keys, cache, rank, truth, want_miss=True)
    print(f"\n현재 가중치 {CURRENT}: {acc*100:.1f}% "
          f"({int(acc*len(keys))}/{len(keys)})")
    for t, g, w in miss:
        print(f"    ✗ {t}: {g} (기대 {w})")

    grid = [(a, b, c) for a in range(6) for b in range(4) for c in range(5)
            if (a, b, c) != (0, 0, 0)]
    best = max(grid, key=lambda p: accuracy(p, keys, cache, rank, truth))
    print(f"\n격자탐색 최적 {best}: "
          f"{accuracy(best, keys, cache, rank, truth)*100:.1f}%")

    # 5-fold 교차검증 — 과적합 여부 확인
    random.seed(42)
    ks = keys[:]
    random.shuffle(ks)
    folds = [ks[i::5] for i in range(5)]
    tot, picks = 0.0, []
    for i in range(5):
        test = folds[i]
        train = [k for j, f in enumerate(folds) if j != i for k in f]
        bp = max(grid, key=lambda p: accuracy(p, train, cache, rank, truth))
        a = accuracy(bp, test, cache, rank, truth)
        tot += a * len(test)
        picks.append(bp)
    print(f"5-fold 교차검증 평균: {tot/len(ks)*100:.1f}%")
    print(f"fold 별 최적: {picks}")
    common = collections.Counter(picks).most_common(1)[0]
    print(f"최빈 파라미터: {common[0]} ({common[1]}/5 fold)")
    if common[0] != CURRENT:
        print(f"\n⚠️ 최빈값이 현재 설정과 다르다. fetch_words.py 의 "
              f"W_ETYM/W_FREQ/W_USAGE 재검토 권장.")
    else:
        print(f"\n✅ 현재 설정이 교차검증 최빈값과 일치")
    return 0


if __name__ == '__main__':
    sys.exit(main())
