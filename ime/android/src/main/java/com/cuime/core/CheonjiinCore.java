package com.cuime.core;

import java.util.*;

/**
 * 천지인 IME 코어 — Android / JVM 포팅
 *
 * 계약 (docs/00-확정스펙.md §8.1):
 *   - 순수 상태 머신. I/O·타이머·전역상태 없음.
 *   - 멀티탭 타임아웃은 셸(InputMethodService)이 소유하고 TIMEOUT 키로 주입한다.
 *   - JS/Python 참조 구현과 동일한 테스트 벡터를 통과해야 한다.
 */
public final class CheonjiinCore {

    /* ───────── 유니코드 한글 ───────── */
    public static final String CHO  = "ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ";
    public static final String JUNG = "ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ";
    public static final String JONG = " ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ";

    public static String compose(String cho, String jung, String jong) {
        int j = (jong == null || jong.isEmpty()) ? 0 : JONG.indexOf(jong);
        return String.valueOf((char) (0xAC00 + (CHO.indexOf(cho) * 21 + JUNG.indexOf(jung)) * 28 + j));
    }

    /** @return {초성, 중성, 종성} 또는 null (한글 음절이 아님) */
    public static String[] decompose(char ch) {
        int c = ch - 0xAC00;
        if (c < 0 || c > 11171) return null;
        return new String[]{
            String.valueOf(CHO.charAt(c / 588)),
            String.valueOf(JUNG.charAt((c % 588) / 28)),
            String.valueOf(JONG.charAt(c % 28)).trim()
        };
    }

    /* ───────── 키맵 (4×4 확정 배치) ───────── */
    public static final Map<String, String> VOWEL_KEYS = new LinkedHashMap<>();
    public static final Map<String, String[]> CONSONANT_CYCLE = new LinkedHashMap<>();
    /** 확정 규칙(docs/00 §3.2): 롱프레스 = 순환열 마지막 항목 */
    public static final Map<String, String> LONGPRESS = new LinkedHashMap<>();

    static {
        VOWEL_KEYS.put("K1", "ㅣ");
        VOWEL_KEYS.put("K2", "ㆍ");
        VOWEL_KEYS.put("K3", "ㅡ");

        CONSONANT_CYCLE.put("K4", new String[]{"ㄱ", "ㅋ", "ㄲ"});
        CONSONANT_CYCLE.put("K5", new String[]{"ㄴ", "ㄹ"});
        CONSONANT_CYCLE.put("K6", new String[]{"ㄷ", "ㅌ", "ㄸ"});
        CONSONANT_CYCLE.put("K7", new String[]{"ㅂ", "ㅍ", "ㅃ"});
        CONSONANT_CYCLE.put("K8", new String[]{"ㅅ", "ㅎ", "ㅆ"});
        CONSONANT_CYCLE.put("K9", new String[]{"ㅈ", "ㅊ", "ㅉ"});
        CONSONANT_CYCLE.put("K0", new String[]{"ㅇ", "ㅁ"});

        for (Map.Entry<String, String[]> e : CONSONANT_CYCLE.entrySet()) {
            String[] cyc = e.getValue();
            LONGPRESS.put(e.getKey(), cyc[cyc.length - 1]);
        }
    }

    /* ───────── 모음 오토마타 ───────── */
    private static final Map<String, Map<String, String>> V = new LinkedHashMap<>();
    private static final Set<String> PENDING = new HashSet<>(Arrays.asList("ㆍ", "ㆍㆍ"));
    private static final Map<String, String> PREV = new LinkedHashMap<>();

    private static void v(String st, String... kv) {
        Map<String, String> m = new LinkedHashMap<>();
        for (int i = 0; i < kv.length; i += 2) m.put(kv[i], kv[i + 1]);
        V.put(st, m);
    }

    static {
        v("",     "ㅣ", "ㅣ", "ㆍ", "ㆍ", "ㅡ", "ㅡ");
        v("ㅣ",   "ㆍ", "ㅏ");
        v("ㆍ",   "ㅣ", "ㅓ", "ㅡ", "ㅗ", "ㆍ", "ㆍㆍ");
        v("ㆍㆍ", "ㅣ", "ㅕ", "ㅡ", "ㅛ", "ㆍ", "ㆍ");
        v("ㅡ",   "ㆍ", "ㅜ", "ㅣ", "ㅢ");
        v("ㅏ",   "ㆍ", "ㅑ", "ㅣ", "ㅐ");
        v("ㅑ",   "ㅣ", "ㅒ", "ㆍ", "ㅏ");
        v("ㅓ",   "ㅣ", "ㅔ");
        v("ㅕ",   "ㅣ", "ㅖ");
        v("ㅐ",   "ㆍ", "ㅒ");
        v("ㅗ",   "ㅣ", "ㅚ");
        v("ㅚ",   "ㆍ", "ㅘ");
        v("ㅘ",   "ㅣ", "ㅙ");
        v("ㅜ",   "ㅣ", "ㅟ", "ㆍ", "ㅠ");
        v("ㅠ",   "ㅣ", "ㅝ");
        v("ㅝ",   "ㅣ", "ㅞ");
        v("ㅟ",   "ㆍ", "ㅝ");
        for (String s : new String[]{"ㅛ", "ㅒ", "ㅔ", "ㅖ", "ㅙ", "ㅞ", "ㅢ"}) v(s);

        for (Map.Entry<String, Map<String, String>> e : V.entrySet())
            for (String nx : e.getValue().values())
                if (!nx.isEmpty() && !PREV.containsKey(nx)) PREV.put(nx, e.getKey());
    }

    /* ───────── 종성 ───────── */
    private static final Map<String, String> JONG_COMBINE = new HashMap<>();
    private static final Map<String, String[]> JONG_SPLIT = new LinkedHashMap<>();
    private static final Set<String> VALID_JONG = new HashSet<>();

    static {
        String[][] cs = {{"ㄱ","ㅅ","ㄳ"},{"ㄴ","ㅈ","ㄵ"},{"ㄴ","ㅎ","ㄶ"},{"ㄹ","ㄱ","ㄺ"},
                         {"ㄹ","ㅁ","ㄻ"},{"ㄹ","ㅂ","ㄼ"},{"ㄹ","ㅅ","ㄽ"},{"ㄹ","ㅌ","ㄾ"},
                         {"ㄹ","ㅍ","ㄿ"},{"ㄹ","ㅎ","ㅀ"},{"ㅂ","ㅅ","ㅄ"}};
        for (String[] c : cs) {
            JONG_COMBINE.put(c[0] + "|" + c[1], c[2]);
            JONG_SPLIT.put(c[2], new String[]{c[0], c[1]});
        }
        for (char c : JONG.trim().toCharArray()) VALID_JONG.add(String.valueOf(c));
    }

    public static Map<String, String[]> jongSplit() { return JONG_SPLIT; }

    /* ───────── 상태 ───────── */
    public static final class State implements Cloneable {
        public String cho = "", vstate = "", jong = "";
        public String lastKey = null;
        public int tap = 0;
        public String[] snap = null;     // {cho, vstate, jong, committed, slot}
        public String committed = "";
        public String slot = "cho";

        @Override public State clone() {
            try {
                State s = (State) super.clone();
                s.snap = (snap == null) ? null : snap.clone();
                return s;
            } catch (CloneNotSupportedException e) { throw new AssertionError(e); }
        }

        /** 조합 중 글자 (preedit) */
        public String preedit() {
            String v = PENDING.contains(vstate) ? "" : vstate;
            if (!cho.isEmpty() && !v.isEmpty()) return compose(cho, v, jong);
            if (!cho.isEmpty()) return cho;
            return v;
        }

        public String text() { return committed + preedit(); }
    }

    public static State newState() { return new State(); }

    private static State flush(State s) {
        State t = s.clone();
        t.committed = s.committed + s.preedit();
        t.cho = ""; t.vstate = ""; t.jong = "";
        t.lastKey = null; t.tap = 0; t.snap = null; t.slot = "cho";
        return t;
    }

    private static String[] snapshot(State s) {
        return new String[]{s.cho, s.vstate, s.jong, s.committed, s.slot};
    }

    private static State restore(State s, String[] snap) {
        State t = s.clone();
        t.cho = snap[0]; t.vstate = snap[1]; t.jong = snap[2];
        t.committed = snap[3]; t.slot = snap[4];
        return t;
    }

    /** 자음 c 를 현재 조합에 결합 */
    private static State attach(State s, String c) {
        State t;
        if (s.slot.equals("cho") && s.cho.isEmpty()) {
            t = s.clone(); t.cho = c; t.slot = "cho"; return t;
        }
        if (s.slot.equals("jong") && !s.jong.isEmpty()) {
            String comb = JONG_COMBINE.get(s.jong + "|" + c);
            if (comb != null) { t = s.clone(); t.jong = comb; t.slot = "jong"; return t; }
            t = flush(s); t.cho = c; t.slot = "cho"; return t;
        }
        if (!s.cho.isEmpty() && !s.vstate.isEmpty() && !PENDING.contains(s.vstate)) {
            if (VALID_JONG.contains(c)) { t = s.clone(); t.jong = c; t.slot = "jong"; return t; }
            t = flush(s); t.cho = c; t.slot = "cho"; return t;
        }
        t = flush(s); t.cho = c; t.slot = "cho"; return t;
    }

    /* ───────── press ───────── */
    public static State press(State s, String key) {
        State t;

        // 모음
        if (VOWEL_KEYS.containsKey(key)) {
            String jamo = VOWEL_KEYS.get(key);
            if (!s.jong.isEmpty()) {                      // 연음(도깨비불)
                String moved, rest;
                String[] sp = JONG_SPLIT.get(s.jong);
                if (sp != null) { moved = sp[1]; rest = sp[0]; }
                else { moved = s.jong; rest = ""; }
                State b = s.clone(); b.jong = rest;
                b = flush(b);
                b.cho = moved; b.slot = "jung";
                s = b;
            }
            Map<String, String> tr = V.get(s.vstate);
            String nxt = (tr == null) ? null : tr.get(jamo);
            if (nxt == null) { s = flush(s); nxt = V.get("").get(jamo); }
            t = s.clone();
            t.vstate = nxt; t.lastKey = null; t.tap = 0; t.snap = null; t.slot = "jung";
            return t;
        }

        // 자음 멀티탭 (스냅샷 되감기)
        if (CONSONANT_CYCLE.containsKey(key)) {
            String[] cyc = CONSONANT_CYCLE.get(key);
            int tap; State base;
            if (key.equals(s.lastKey) && s.snap != null) {
                tap = (s.tap + 1) % cyc.length;
                base = restore(s, s.snap);
            } else { tap = 0; base = s; }
            String[] snap = snapshot(base);
            t = attach(base, cyc[tap]);
            t.lastKey = key; t.tap = tap; t.snap = snap;
            return t;
        }

        // 롱프레스 = 순환열 마지막
        if (key.startsWith("LONG_")) {
            String k = key.substring(5);
            if (!CONSONANT_CYCLE.containsKey(k)) return s;
            t = attach(s, LONGPRESS.get(k));
            t.lastKey = null; t.tap = 0; t.snap = null;
            return t;
        }

        // 방향키 — 조합 중이면 확정
        if (key.equals("KRIGHT") || key.equals("KLEFT")) {
            if (!s.preedit().isEmpty()) return flush(s);
            t = s.clone(); t.lastKey = null; t.tap = 0; t.snap = null; return t;
        }

        if (key.equals("TIMEOUT")) {
            t = s.clone(); t.lastKey = null; t.tap = 0; t.snap = null; return t;
        }
        if (key.equals("SPACE")) { t = flush(s); t.committed += " ";  return t; }
        if (key.equals("ENTER")) { t = flush(s); t.committed += "\n"; return t; }
        if (key.equals("COMMIT")) return flush(s);
        if (key.equals("BACK")) return backspace(s);
        if (key.startsWith("CHAR_")) { t = flush(s); t.committed += key.substring(5); return t; }

        throw new IllegalArgumentException("unknown key: " + key);
    }

    /** 자모 단위 역순 삭제 */
    public static State backspace(State s) {
        State t;
        if (!s.jong.isEmpty()) {
            t = s.clone();
            String[] sp = JONG_SPLIT.get(s.jong);
            t.jong = (sp != null) ? sp[0] : "";
            if (t.jong.isEmpty()) t.slot = "jung";
            t.lastKey = null; t.tap = 0; t.snap = null;
            return t;
        }
        if (!s.vstate.isEmpty()) {
            String prev = PREV.containsKey(s.vstate) ? PREV.get(s.vstate) : "";
            t = s.clone();
            t.vstate = prev; t.slot = prev.isEmpty() ? "cho" : "jung";
            t.lastKey = null; t.tap = 0; t.snap = null;
            return t;
        }
        if (!s.cho.isEmpty()) {
            t = s.clone(); t.cho = ""; t.slot = "cho";
            t.lastKey = null; t.tap = 0; t.snap = null;
            return t;
        }
        if (!s.committed.isEmpty()) {
            t = s.clone();
            t.committed = s.committed.substring(0, s.committed.length() - 1);
            return t;
        }
        return s;
    }

    public static String typeKeys(List<String> keys) {
        State s = newState();
        for (String k : keys) s = press(s, k);
        return flush(s).committed;
    }

    /* ───────── 역방향: 텍스트 → 키 시퀀스 ───────── */
    private static final Map<String, Object[]> KEY_OF_JAMO = new HashMap<>();
    private static final Map<String, String> INV_VOWEL = new HashMap<>();
    private static final Map<String, List<String>> VSEQ_CACHE = new HashMap<>();

    static {
        for (Map.Entry<String, String[]> e : CONSONANT_CYCLE.entrySet()) {
            String[] cyc = e.getValue();
            for (int i = 0; i < cyc.length; i++)
                if (!KEY_OF_JAMO.containsKey(cyc[i]))
                    KEY_OF_JAMO.put(cyc[i], new Object[]{e.getKey(), i + 1});
        }
        for (Map.Entry<String, String> e : VOWEL_KEYS.entrySet())
            INV_VOWEL.put(e.getValue(), e.getKey());
    }

    /** 모음 → 최단 키 시퀀스 (BFS) */
    public static List<String> vowelSeq(String v) {
        List<String> c = VSEQ_CACHE.get(v);
        if (c != null) return new ArrayList<>(c);
        Deque<Object[]> q = new ArrayDeque<>();
        q.add(new Object[]{"", new ArrayList<String>()});
        Set<String> seen = new HashSet<>(Collections.singletonList(""));
        while (!q.isEmpty()) {
            Object[] cur = q.poll();
            String st = (String) cur[0];
            @SuppressWarnings("unchecked") List<String> path = (List<String>) cur[1];
            if (st.equals(v)) { VSEQ_CACHE.put(v, path); return new ArrayList<>(path); }
            Map<String, String> tr = V.get(st);
            if (tr == null) continue;
            for (Map.Entry<String, String> e : tr.entrySet()) {
                String nx = e.getValue();
                if (!nx.isEmpty() && !seen.contains(nx)) {
                    seen.add(nx);
                    List<String> np = new ArrayList<>(path);
                    np.add(INV_VOWEL.get(e.getKey()));
                    q.add(new Object[]{nx, np});
                }
            }
        }
        return new ArrayList<>();
    }

    private static Object[] consSeq(String jamo, boolean useLongpress) {
        Object[] kt = KEY_OF_JAMO.get(jamo);
        String k = (String) kt[0]; int taps = (Integer) kt[1];
        List<String> seq = new ArrayList<>();
        if (useLongpress && LONGPRESS.get(k).equals(jamo)) {
            seq.add("LONG_" + k);
            return new Object[]{seq, null};
        }
        for (int i = 0; i < taps; i++) seq.add(k);
        return new Object[]{seq, k};
    }

    public static List<String> encode(String str, boolean useLongpress) {
        List<String> keys = new ArrayList<>();
        String lastKey = null;
        for (int i = 0; i < str.length(); i++) {
            char ch = str.charAt(i);
            if (ch == ' ') { keys.add("SPACE"); lastKey = null; continue; }
            if (ch == '\n') { keys.add("ENTER"); lastKey = null; continue; }
            String[] d = decompose(ch);
            if (d == null) { keys.add("CHAR_" + ch); lastKey = null; continue; }

            List<String> sy = new ArrayList<>();
            Object[] r = consSeq(d[0], useLongpress);
            @SuppressWarnings("unchecked") List<String> cs = (List<String>) r[0];
            sy.addAll(cs);
            String syLast = (String) r[1];
            sy.addAll(vowelSeq(d[1]));
            syLast = null;
            if (!d[2].isEmpty()) {
                String[] sp = JONG_SPLIT.get(d[2]);
                List<String> parts = (sp != null) ? Arrays.asList(sp) : Collections.singletonList(d[2]);
                for (String p : parts) {
                    r = consSeq(p, useLongpress);
                    @SuppressWarnings("unchecked") List<String> ps = (List<String>) r[0];
                    String pk = (String) r[1];
                    if (pk != null && pk.equals(syLast)) sy.add("KRIGHT");
                    sy.addAll(ps);
                    syLast = pk;
                }
            }
            if (!sy.isEmpty() && sy.get(0).equals(lastKey)) keys.add("KRIGHT");
            keys.addAll(sy);
            lastKey = syLast;
        }
        return keys;
    }

    private CheonjiinCore() {}
}
