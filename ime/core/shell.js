/*!
 * 천지인 IME 셸 계층 — 코어(cheonjiin.js) 바깥의 시간·UI 상태 담당
 *
 * 책임 (docs/02 §1.1):
 *   - 멀티탭 타임아웃 소유 → 코어에 TIMEOUT 주입
 *   - 롱프레스 판정 (기본 300ms)
 *   - 漢 키 3중 의미 판정 (docs/06 §2)
 *   - 레이아웃 전환 (한글/영어/숫자)
 *   - M1 전송 가드 (docs/07 §2.3)
 */
(function (root, factory) {
  const api = factory(typeof require === 'function' ? require('./cheonjiin.js') : window.Cheonjiin);
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  if (typeof window !== 'undefined') window.CheonjiinShell = api;
})(this, function (C) {
  'use strict';

  const LAYOUTS = ['한글', '영어', '숫자'];

  const DEFAULTS = {
    multitapMs: 800,     // 멀티탭 타임아웃 (docs/05)
    longpressMs: 300,    // 롱프레스 (docs/07 §4 — 실사용 조정 대상)
    chordMinMs: 60,      // 코드 최소 겹침 (docs/07 §3.3)
    sendGuardMs: 300,    // M1 전송 가드 (docs/07 §2.3)
    sequentialChordMs: 500,
    enterIsSend: false,  // 메신저형 컨텍스트 여부
  };

  // E.161 표준 (docs/06 §5.2)
  const ENGLISH = {
    K1: ['.', ',', '?', '!', "'"],
    K2: ['a', 'b', 'c'], K3: ['d', 'e', 'f'], K4: ['g', 'h', 'i'],
    K5: ['j', 'k', 'l'], K6: ['m', 'n', 'o'], K7: ['p', 'q', 'r', 's'],
    K8: ['t', 'u', 'v'], K9: ['w', 'x', 'y', 'z'],
  };
  const NUMBER = { K1: '1', K2: '2', K3: '3', K4: '4', K5: '5', K6: '6',
                   K7: '7', K8: '8', K9: '9', K0: '0' };

  const SYMBOLS = [
    ['.', ',', '?', '!', '~', '…', "'", '"', '(', ')', ':', ';', '-', '/', '@', '#'],
    ['+', '-', '×', '÷', '=', '±', '%', '°', '℃', '₩', '$', '€', '¥', '№', '※', '★'],
    ['[', ']', '{', '}', '〈', '〉', '《', '》', '「', '」', '←', '→', '↑', '↓', '↔', '⇒'],
  ];

  // 한자 사전 — tools/gen_hanja.py 가 생성한 실제 사전 (KS X 1001 4,888자 + 단어 227)
  const HJ = (typeof require === 'function')
    ? require('./hanja.js')
    : (typeof window !== 'undefined' ? window.HanjaDict : null);
  function hanjaLookup(t) { return HJ ? HJ.lookup(t) : []; }

  function create(opts) {
    const cfg = Object.assign({}, DEFAULTS, opts || {});
    const listeners = [];

    const st = {
      core: C.newState(),
      layout: '한글',
      shift: false,

      // 漢 키 코드 판정
      hjDown: false, hjDownAt: 0, hjConsumed: false,

      // 후보/팔레트
      candidates: null, candIndex: 0, candTarget: null,
      paletteOpen: false, palettePage: 0,

      // 영어/숫자 멀티탭
      enKey: null, enTap: 0, enChar: '',

      // M1 전송 가드
      lastBackAt: 0, guardBlocked: false,

      // 타이머
      _mt: null, _lp: null, _lpFired: false,
      selection: null,       // { text, isHangul }
      lastMessage: '',
    };

    const emit = () => { const v = view(); listeners.forEach((f) => f(v)); };
    const on = (f) => { listeners.push(f); return () => listeners.splice(listeners.indexOf(f), 1); };

    function view() {
      return {
        text: C.text(st.core),
        committed: st.core.committed,
        preedit: C.preedit(st.core),
        layout: st.layout,
        shift: st.shift,
        candidates: st.candidates,
        candIndex: st.candIndex,
        paletteOpen: st.paletteOpen,
        palettePage: st.palettePage,
        symbols: SYMBOLS[st.palettePage],
        activeKey: st.core.lastKey,
        tap: st.core.tap,
        cycleLen: st.core.lastKey ? (C.CONSONANT_CYCLE[st.core.lastKey] || []).length : 0,
        enKey: st.enKey, enTap: st.enTap,
        message: st.lastMessage,
      };
    }

    /* ── 타이머 ── */
    function clearMultitap() { if (st._mt) { clearTimeout(st._mt); st._mt = null; } }
    function clearLongpress() { if (st._lp) { clearTimeout(st._lp); st._lp = null; } st._lpFired = false; }
    function armMultitap() {
      clearMultitap();
      st._mt = setTimeout(() => {
        st._mt = null;
        st.core = C.press(st.core, 'TIMEOUT');
        st.enKey = null; st.enTap = 0;
        emit();
      }, cfg.multitapMs);
    }

    /* ── 코어 위임 ── */
    function core(key) { st.core = C.press(st.core, key); }

    /* ── 영어/숫자 멀티탭 (셸에서 처리) ── */
    function pressEnglish(k, isLong) {
      const cyc = ENGLISH[k];
      if (!cyc) {                                   // K0 = shift
        if (k === 'K0') { st.shift = !st.shift; return; }
        return;
      }
      if (isLong) {                                 // 롱프레스 = 순환 마지막
        commitEnChar();
        let ch = cyc[cyc.length - 1];
        if (st.shift) { ch = ch.toUpperCase(); st.shift = false; }
        core('CHAR_' + ch);
        st.enKey = null; st.enTap = 0; st.enChar = '';
        return;
      }
      if (st.enKey === k) {
        st.core = C.press(st.core, 'BACK');         // 직전 글자 되돌리고
        st.enTap = (st.enTap + 1) % cyc.length;
      } else {
        commitEnChar();
        st.enKey = k; st.enTap = 0;
      }
      let ch = cyc[st.enTap];
      if (st.shift) { ch = ch.toUpperCase(); st.shift = false; }
      st.enChar = ch;
      core('CHAR_' + ch);
    }
    function commitEnChar() { st.enKey = null; st.enTap = 0; st.enChar = ''; }

    /* ── 漢 키 (docs/06 §2) ── */
    function openCandidates() {
      const sel = st.selection;
      let target = null, list = null;
      if (sel && sel.text && sel.isHangul) {
        target = sel.text;
        const r = hanjaLookup(target);
        list = r.length ? r : null;
      } else {
        const pre = C.preedit(st.core);
        const cm = st.core.committed;
        const ch = pre || (cm ? cm[cm.length - 1] : '');
        if (ch && C.decompose(ch)) {
          target = ch;
          const r = hanjaLookup(ch);
          list = r.length ? r : null;
        }
      }
      if (list && list.length) {
        st.candidates = list; st.candIndex = 0; st.candTarget = target;
        st.paletteOpen = false;
        return true;
      }
      return false;
    }

    function applyCandidate() {
      if (!st.candidates) return;
      const pick = st.candidates[st.candIndex];
      const n = st.candTarget ? st.candTarget.length : 1;
      if (st.selection) {
        st.selection = null;
        st.core = C.flush(st.core);
        st.core.committed = st.core.committed.slice(0, -n) + pick;
      } else {
        st.core = C.flush(st.core);
        st.core.committed = st.core.committed.slice(0, -n) + pick;
      }
      st.candidates = null; st.candIndex = 0; st.candTarget = null;
      st.lastMessage = '한자 변환: ' + pick;
    }

    /* ── 공개 API ── */
    const api = {
      view, on,
      config: cfg,
      setSelection(text, isHangul) {
        st.selection = text ? { text, isHangul: isHangul !== false } : null;
        emit();
      },

      /** 글자키 누름 시작 (롱프레스 타이머 시작) */
      keyDown(k) {
        st._lpFired = false;
        if (st._lp) clearTimeout(st._lp);
        st._lp = setTimeout(() => {
          st._lp = null; st._lpFired = true;
          api.longPress(k);
        }, cfg.longpressMs);
      },

      /** 글자키 뗌 — 롱프레스가 이미 발동했으면 무시 */
      keyUp(k) {
        if (st._lp) { clearTimeout(st._lp); st._lp = null; }
        if (st._lpFired) { st._lpFired = false; return; }
        api.tap(k);
      },

      tap(k) {
        st.lastMessage = '';
        if (st.paletteOpen) return;
        if (st.candidates) { st.candidates = null; }   // 후보 취소 후 계속 입력
        if (st.layout === '한글') { core(k); armMultitap(); }
        else if (st.layout === '영어') { pressEnglish(k, false); armMultitap(); }
        else { const d = NUMBER[k]; if (d) { commitEnChar(); core('CHAR_' + d); } }
        emit();
      },

      longPress(k) {
        st.lastMessage = '';
        if (st.paletteOpen) return;
        if (st.layout === '한글') {
          if (k in C.CONSONANT_CYCLE) { clearMultitap(); core('LONG_' + k); }
          else return;
        } else if (st.layout === '영어') { clearMultitap(); pressEnglish(k, true); }
        else return;
        emit();
      },

      /* 漢 키 — down/up 분리 (코드 판정) */
      hanjaDown() { st.hjDown = true; st.hjDownAt = Date.now(); st.hjConsumed = false; },

      hanjaUp() {
        const wasConsumed = st.hjConsumed;
        st.hjDown = false; st.hjConsumed = false;
        if (wasConsumed) { emit(); return; }          // 코드로 소비됨 → 팔레트 안 뜸
        if (st.candidates) {                           // 다시 누르면 닫기
          st.candidates = null; emit(); return;
        }
        if (st.paletteOpen) { st.paletteOpen = false; emit(); return; }
        clearMultitap();
        st.core = C.press(st.core, 'TIMEOUT');
        if (!openCandidates()) { st.paletteOpen = true; st.palettePage = 0; }
        emit();
      },

      hanjaLongPress() {                               // 기호 강제
        st.hjConsumed = true;
        st.candidates = null;
        st.paletteOpen = true; st.palettePage = 0;
        emit();
      },

      /* 방향키 — 문맥에 따라 4가지 (docs/06 §2.3) */
      arrow(dir) {                                     // dir: -1 왼쪽, +1 오른쪽
        st.lastMessage = '';
        // (1) 코드 → 레이아웃 전환
        if (st.hjDown) {
          if (Date.now() - st.hjDownAt >= cfg.chordMinMs || true) {
            if (!st.hjConsumed) {
              const i = LAYOUTS.indexOf(st.layout);
              st.layout = LAYOUTS[(i + dir + LAYOUTS.length) % LAYOUTS.length];
              st.hjConsumed = true;
              st.candidates = null; st.paletteOpen = false;
              clearMultitap(); clearLongpress(); commitEnChar();
              st.core = C.press(st.core, 'COMMIT');
              st.lastMessage = st.layout + ' 레이아웃';
            }
          }
          emit(); return;
        }
        // (2) 후보 탐색
        if (st.candidates) {
          const n = st.candidates.length;
          st.candIndex = (st.candIndex + dir + n) % n;
          emit(); return;
        }
        // (3) 팔레트 페이지
        if (st.paletteOpen) {
          st.palettePage = (st.palettePage + dir + SYMBOLS.length) % SYMBOLS.length;
          emit(); return;
        }
        // (4) 조합 확정 / 커서 이동
        clearMultitap(); commitEnChar();
        core(dir > 0 ? 'KRIGHT' : 'KLEFT');
        emit();
      },

      /* 후보/기호 직접 선택 */
      pickCandidate(i) {
        if (!st.candidates) return;
        st.candIndex = i; applyCandidate(); emit();
      },
      pickSymbol(ch, keepOpen) {
        clearMultitap(); commitEnChar();
        core('CHAR_' + ch);
        if (!keepOpen) st.paletteOpen = false;
        emit();
      },

      backspace() {
        st.lastMessage = '';
        clearMultitap(); commitEnChar();
        if (st.candidates) { st.candidates = null; emit(); return; }
        if (st.paletteOpen) { st.paletteOpen = false; emit(); return; }
        st.lastBackAt = Date.now();                    // M1 가드 기록
        core('BACK');
        emit();
      },

      space() {
        clearMultitap(); commitEnChar();
        if (st.candidates) { applyCandidate(); emit(); return; }
        core('SPACE'); emit();
      },

      /** 엔터 — M1 전송 가드 (docs/07 §2.3) */
      enter() {
        clearMultitap(); commitEnChar();
        if (st.candidates) { applyCandidate(); emit(); return; }
        if (st.paletteOpen) { st.paletteOpen = false; emit(); return; }
        const since = Date.now() - st.lastBackAt;
        if (cfg.enterIsSend && st.lastBackAt && since < cfg.sendGuardMs) {
          st.guardBlocked = true;
          st.lastBackAt = 0;
          st.lastMessage = `⚠ 전송 차단 — ⌫ 직후 ${since}ms (M1 가드)`;
          emit(); return;
        }
        core('ENTER'); emit();
      },

      reset() { st.core = C.newState(); st.candidates = null; st.paletteOpen = false;
                st.lastMessage = ''; clearMultitap(); clearLongpress(); commitEnChar(); emit(); },
      _state: st,
      LAYOUTS, ENGLISH, NUMBER, SYMBOLS,
    };
    return api;
  }

  return { create, LAYOUTS, DEFAULTS, ENGLISH, NUMBER, SYMBOLS };
});
