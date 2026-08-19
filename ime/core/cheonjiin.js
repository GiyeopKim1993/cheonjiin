/*!
 * 천지인 IME 코어 — Cheonjiin Unified IME (CU-IME)
 * 4×4 (16키) 배열 / 순수 상태 머신 / 의존성 없음
 *
 * 설계 계약 (docs/02-기술스펙.md §1.1):
 *   - 코어는 시간을 모른다. 타이머는 셸이 소유하며 TIMEOUT/COMMIT 키로 주입한다.
 *   - press(state, key) -> state 는 순수 함수. I/O·전역상태 없음.
 *
 * 브라우저: window.Cheonjiin
 * Node    : module.exports
 */
(function (root, factory) {
  const api = factory();
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  if (typeof window !== 'undefined') window.Cheonjiin = api;
})(this, function () {
  'use strict';

  /* ───────────────────────── 유니코드 한글 ───────────────────────── */
  const CHO  = 'ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ';
  const JUNG = 'ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ';
  const JONG = ' ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ';

  function compose(cho, jung, jong) {
    return String.fromCharCode(
      0xac00 + (CHO.indexOf(cho) * 21 + JUNG.indexOf(jung)) * 28 + JONG.indexOf(jong || ' ')
    );
  }
  function decompose(ch) {
    const c = ch.charCodeAt(0) - 0xac00;
    if (c < 0 || c > 11171) return null;
    return [CHO[Math.floor(c / 588)], JUNG[Math.floor((c % 588) / 28)], JONG[c % 28].trim()];
  }

  /* ───────────────────────── 키맵 (4×4) ───────────────────────── */
  // 글자키 10개. 기능키(⌫ ↵ 漢 ␣)와 방향키(◀ ▶)는 셸이 처리.
  const VOWEL_KEYS = { K1: 'ㅣ', K2: 'ㆍ', K3: 'ㅡ' };

  const CONSONANT_CYCLE = {
    K4: ['ㄱ', 'ㅋ', 'ㄲ'],
    K5: ['ㄴ', 'ㄹ'],
    K6: ['ㄷ', 'ㅌ', 'ㄸ'],
    K7: ['ㅂ', 'ㅍ', 'ㅃ'],
    K8: ['ㅅ', 'ㅎ', 'ㅆ'],
    K9: ['ㅈ', 'ㅊ', 'ㅉ'],
    K0: ['ㅇ', 'ㅁ'],
  };

  // 확정 규칙(docs/07 §3.1): 롱프레스 = 순환열의 마지막 항목
  const LONGPRESS = {};
  for (const k in CONSONANT_CYCLE) {
    const cyc = CONSONANT_CYCLE[k];
    LONGPRESS[k] = cyc[cyc.length - 1];
  }

  /* ───────────────────────── 모음 오토마타 ───────────────────────── */
  const V = {
    '':     { 'ㅣ': 'ㅣ', 'ㆍ': 'ㆍ', 'ㅡ': 'ㅡ' },
    'ㅣ':   { 'ㆍ': 'ㅏ' },
    'ㆍ':   { 'ㅣ': 'ㅓ', 'ㅡ': 'ㅗ', 'ㆍ': 'ㆍㆍ' },
    'ㆍㆍ': { 'ㅣ': 'ㅕ', 'ㅡ': 'ㅛ', 'ㆍ': 'ㆍ' },
    'ㅡ':   { 'ㆍ': 'ㅜ', 'ㅣ': 'ㅢ' },
    'ㅏ':   { 'ㆍ': 'ㅑ', 'ㅣ': 'ㅐ' },
    'ㅑ':   { 'ㅣ': 'ㅒ', 'ㆍ': 'ㅏ' },
    'ㅓ':   { 'ㅣ': 'ㅔ' },
    'ㅕ':   { 'ㅣ': 'ㅖ' },
    'ㅐ':   { 'ㆍ': 'ㅒ' },
    'ㅗ':   { 'ㅣ': 'ㅚ' },
    'ㅚ':   { 'ㆍ': 'ㅘ' },
    'ㅘ':   { 'ㅣ': 'ㅙ' },
    'ㅜ':   { 'ㅣ': 'ㅟ', 'ㆍ': 'ㅠ' },
    'ㅠ':   { 'ㅣ': 'ㅝ' },
    'ㅝ':   { 'ㅣ': 'ㅞ' },
    'ㅟ':   { 'ㆍ': 'ㅝ' },
    'ㅛ': {}, 'ㅒ': {}, 'ㅔ': {}, 'ㅖ': {}, 'ㅙ': {}, 'ㅞ': {}, 'ㅢ': {},
  };
  const PENDING = new Set(['ㆍ', 'ㆍㆍ']);   // 화면에 출력되지 않는 중간 상태

  // 백스페이스 역추적용 (BFS 최단 경로의 부모)
  const PREV = {};
  (function buildPrev() {
    for (const st in V) for (const j in V[st]) {
      const nx = V[st][j];
      if (nx && !(nx in PREV)) PREV[nx] = st;
    }
  })();

  /* ───────────────────────── 종성 ───────────────────────── */
  const JONG_COMBINE = {
    'ㄱ|ㅅ': 'ㄳ', 'ㄴ|ㅈ': 'ㄵ', 'ㄴ|ㅎ': 'ㄶ', 'ㄹ|ㄱ': 'ㄺ', 'ㄹ|ㅁ': 'ㄻ',
    'ㄹ|ㅂ': 'ㄼ', 'ㄹ|ㅅ': 'ㄽ', 'ㄹ|ㅌ': 'ㄾ', 'ㄹ|ㅍ': 'ㄿ', 'ㄹ|ㅎ': 'ㅀ',
    'ㅂ|ㅅ': 'ㅄ',
  };
  const JONG_SPLIT = {};
  for (const k in JONG_COMBINE) JONG_SPLIT[JONG_COMBINE[k]] = k.split('|');
  const VALID_JONG = new Set(JONG.trim().split(''));

  /* ───────────────────────── 상태 ───────────────────────── */
  function newState() {
    return {
      cho: '', vstate: '', jong: '',
      lastKey: null, tap: 0, snap: null,
      committed: '', slot: 'cho',
    };
  }
  const clone = (s) => Object.assign({}, s);

  function preedit(s) {
    const v = PENDING.has(s.vstate) ? '' : s.vstate;
    if (s.cho && v) return compose(s.cho, v, s.jong);
    if (s.cho) return s.cho;
    if (v) return v;
    return '';
  }
  const text = (s) => s.committed + preedit(s);

  function flush(s) {
    const t = clone(s);
    t.committed = s.committed + preedit(s);
    t.cho = ''; t.vstate = ''; t.jong = '';
    t.lastKey = null; t.tap = 0; t.snap = null; t.slot = 'cho';
    return t;
  }

  const snapshot = (s) => [s.cho, s.vstate, s.jong, s.committed, s.slot];
  function restore(s, snap) {
    const t = clone(s);
    [t.cho, t.vstate, t.jong, t.committed, t.slot] = snap;
    return t;
  }

  /** 자음 c 를 현재 조합에 결합 (멀티탭 tap 관리는 호출측 책임) */
  function attach(s, c) {
    let t;
    if (s.slot === 'cho' && !s.cho) {
      t = clone(s); t.cho = c; t.slot = 'cho'; return t;
    }
    // 종성이 이미 있음 → 겹받침 시도
    if (s.slot === 'jong' && s.jong) {
      const comb = JONG_COMBINE[s.jong + '|' + c];
      if (comb) { t = clone(s); t.jong = comb; t.slot = 'jong'; return t; }
      t = flush(s); t.cho = c; t.slot = 'cho'; return t;
    }
    // 초성+중성 완성 → 종성 자리
    if (s.cho && s.vstate && !PENDING.has(s.vstate)) {
      if (VALID_JONG.has(c)) { t = clone(s); t.jong = c; t.slot = 'jong'; return t; }
      t = flush(s); t.cho = c; t.slot = 'cho'; return t;
    }
    t = flush(s); t.cho = c; t.slot = 'cho'; return t;
  }

  function pullJong(jong) {
    if (JONG_SPLIT[jong]) return [JONG_SPLIT[jong][1], JONG_SPLIT[jong][0]];
    return [jong, ''];
  }

  /* ───────────────────────── press ───────────────────────── */
  function press(s, key) {
    let t;

    /* 모음 */
    if (key in VOWEL_KEYS) {
      const jamo = VOWEL_KEYS[key];
      // 받침 뒤 모음 → 연음(도깨비불)
      if (s.jong) {
        const [moved, rest] = pullJong(s.jong);
        let b = clone(s); b.jong = rest;
        b = flush(b);
        b.cho = moved; b.slot = 'jung';
        s = b;
      }
      let nxt = (V[s.vstate] || {})[jamo];
      if (nxt === undefined) { s = flush(s); nxt = V[''][jamo]; }
      t = clone(s);
      t.vstate = nxt; t.lastKey = null; t.tap = 0; t.snap = null; t.slot = 'jung';
      return t;
    }

    /* 자음 (멀티탭 — 스냅샷 되감기) */
    if (key in CONSONANT_CYCLE) {
      const cyc = CONSONANT_CYCLE[key];
      let tap, base;
      if (s.lastKey === key && s.snap !== null) {
        tap = (s.tap + 1) % cyc.length;
        base = restore(s, s.snap);
      } else {
        tap = 0; base = s;
      }
      const snap = snapshot(base);
      t = attach(base, cyc[tap]);
      t.lastKey = key; t.tap = tap; t.snap = snap;
      return t;
    }

    /* 롱프레스 = 순환열 마지막 항목 (docs/07 §3.1) */
    if (key.indexOf('LONG_') === 0) {
      const k = key.slice(5);
      if (!(k in CONSONANT_CYCLE)) return s;
      t = attach(s, LONGPRESS[k]);
      t.lastKey = null; t.tap = 0; t.snap = null;   // 멀티탭 세션 종료
      return t;
    }

    /* 방향키 — 조합 중이면 확정, 아니면 세션만 종료 (docs/05 §3.2) */
    if (key === 'KRIGHT' || key === 'KLEFT') {
      if (preedit(s)) return flush(s);
      t = clone(s); t.lastKey = null; t.tap = 0; t.snap = null; return t;
    }

    /* 셸이 주입하는 타임아웃 — 멀티탭 세션만 종료 */
    if (key === 'TIMEOUT') {
      t = clone(s); t.lastKey = null; t.tap = 0; t.snap = null; return t;
    }

    if (key === 'SPACE')  { t = flush(s); t.committed += ' ';  return t; }
    if (key === 'ENTER')  { t = flush(s); t.committed += '\n'; return t; }
    if (key === 'COMMIT') return flush(s);
    if (key === 'BACK')   return backspace(s);

    /* 임의 문자 직접 삽입 (기호/영문/숫자 레이어) */
    if (key.indexOf('CHAR_') === 0) {
      t = flush(s); t.committed += key.slice(5); return t;
    }
    throw new Error('unknown key: ' + key);
  }

  /* 자모 단위 역순 삭제 (docs/02 §5.4) */
  function backspace(s) {
    let t;
    if (s.jong) {
      t = clone(s);
      t.jong = JONG_SPLIT[s.jong] ? JONG_SPLIT[s.jong][0] : '';
      if (!t.jong) t.slot = 'jung';
      t.lastKey = null; t.tap = 0; t.snap = null;
      return t;
    }
    if (s.vstate) {
      const prev = PREV[s.vstate] !== undefined ? PREV[s.vstate] : '';
      t = clone(s);
      t.vstate = prev; t.slot = prev ? 'jung' : 'cho';
      t.lastKey = null; t.tap = 0; t.snap = null;
      return t;
    }
    if (s.cho) {
      t = clone(s); t.cho = ''; t.slot = 'cho';
      t.lastKey = null; t.tap = 0; t.snap = null;
      return t;
    }
    if (s.committed) {
      t = clone(s); t.committed = s.committed.slice(0, -1); return t;
    }
    return s;
  }

  /* 편의: 키 배열 → 확정 문자열 */
  function typeKeys(keys) {
    let s = newState();
    for (const k of keys) s = press(s, k);
    return flush(s).committed;
  }

  /* ───────────────────── 텍스트 → 키 시퀀스 (역방향) ───────────────────── */
  const KEY_OF_JAMO = {};
  for (const k in CONSONANT_CYCLE) {
    CONSONANT_CYCLE[k].forEach((c, i) => {
      if (!(c in KEY_OF_JAMO)) KEY_OF_JAMO[c] = [k, i + 1];
    });
  }
  const INV_VOWEL = {}; for (const k in VOWEL_KEYS) INV_VOWEL[VOWEL_KEYS[k]] = k;

  const _vseqCache = {};
  function vowelSeq(v) {                       // BFS 최단 경로
    if (_vseqCache[v]) return _vseqCache[v].slice();
    const q = [['', []]], seen = new Set(['']);
    while (q.length) {
      const [st, path] = q.shift();
      if (st === v) { _vseqCache[v] = path; return path.slice(); }
      for (const jamo in (V[st] || {})) {
        const nx = V[st][jamo];
        if (nx && !seen.has(nx)) { seen.add(nx); q.push([nx, path.concat(INV_VOWEL[jamo])]); }
      }
    }
    return [];
  }

  /** 음절 → 키 시퀀스. useLongpress 시 순환 마지막 자모를 LONG_ 로 1타 처리 */
  function encodeSyllable(ch, useLongpress) {
    const d = decompose(ch);
    if (!d) return { keys: ['CHAR_' + ch], lastKey: null };
    const [cho, jung, jong] = d;
    const keys = [];
    const cons = (jamo) => {
      const [k, taps] = KEY_OF_JAMO[jamo];
      if (useLongpress && LONGPRESS[k] === jamo) return { seq: ['LONG_' + k], key: null };
      return { seq: new Array(taps).fill(k), key: k };
    };
    let r = cons(cho);
    keys.push.apply(keys, r.seq);
    let lastKey = r.key;
    keys.push.apply(keys, vowelSeq(jung));
    lastKey = null;                                  // 모음이 멀티탭 세션을 끊음
    if (jong) {
      const parts = JONG_SPLIT[jong] ? JONG_SPLIT[jong].slice() : [jong];
      for (const p of parts) {
        r = cons(p);
        if (r.key && r.key === lastKey) keys.push('KRIGHT');   // 멀티탭 충돌 → 확정
        keys.push.apply(keys, r.seq);
        lastKey = r.key;
      }
    }
    return { keys, lastKey };
  }

  function encode(str, useLongpress) {
    const keys = [];
    let lastKey = null;
    for (const ch of str) {
      if (ch === ' ') { keys.push('SPACE'); lastKey = null; continue; }
      if (ch === '\n') { keys.push('ENTER'); lastKey = null; continue; }
      const r = encodeSyllable(ch, useLongpress);
      if (r.keys.length && r.keys[0] === lastKey) keys.push('KRIGHT');
      keys.push.apply(keys, r.keys);
      lastKey = r.lastKey;
    }
    return keys;
  }

  return {
    CHO, JUNG, JONG, compose, decompose,
    VOWEL_KEYS, CONSONANT_CYCLE, LONGPRESS, V, PENDING,
    JONG_COMBINE, JONG_SPLIT,
    newState, press, backspace, preedit, text, flush, typeKeys,
    vowelSeq, encode, encodeSyllable, KEY_OF_JAMO,
  };
});
