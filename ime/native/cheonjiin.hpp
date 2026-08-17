// 천지인 IME 코어 — C++ 포팅 (Windows TSF / Linux IBus / macOS IMK 공용)
//
// 계약 (docs/00-확정스펙.md §8.1):
//   - 순수 상태 머신. I/O·타이머·전역상태 없음.
//   - 타임아웃은 셸이 소유하고 "TIMEOUT" 키로 주입한다.
//   - JS/Java/Python 참조 구현과 동일한 테스트 벡터를 통과해야 한다.
//
// 자모는 UTF-8 std::string 으로 다룬다 (한글 자모는 3바이트).
#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <algorithm>

namespace cuime {

using std::string;
using std::vector;

/* ───────── UTF-8 유틸 ───────── */
inline vector<string> utf8chars(const string& s) {
    vector<string> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        size_t len = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        out.push_back(s.substr(i, len));
        i += len;
    }
    return out;
}
inline string fromCodepoint(unsigned cp) {
    string o;
    if (cp < 0x80) o += char(cp);
    else if (cp < 0x800) { o += char(0xC0 | (cp >> 6)); o += char(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) {
        o += char(0xE0 | (cp >> 12));
        o += char(0x80 | ((cp >> 6) & 0x3F));
        o += char(0x80 | (cp & 0x3F));
    }
    return o;
}
inline unsigned toCodepoint(const string& ch) {
    unsigned char c = ch[0];
    if (c < 0x80) return c;
    if (c < 0xE0) return ((c & 0x1F) << 6) | (ch[1] & 0x3F);
    return ((c & 0x0F) << 12) | ((ch[1] & 0x3F) << 6) | (ch[2] & 0x3F);
}

/* ───────── 테이블 ───────── */
struct Tables {
    vector<string> CHO, JUNG, JONG;
    std::map<string, string> VOWEL_KEYS;
    std::map<string, vector<string>> CONSONANT_CYCLE;
    std::map<string, string> LONGPRESS;
    std::map<string, std::map<string, string>> V;
    vector<std::pair<string, vector<std::pair<string,string>>>> V_ORDER;  // 정의 순서 보존
    std::map<string, vector<std::pair<string,string>>> V_ORD;             // state -> 정의순 전이
    std::set<string> PENDING;
    std::map<string, string> PREV;
    std::map<string, string> JONG_COMBINE;
    std::map<string, std::pair<string, string>> JONG_SPLIT;
    std::set<string> VALID_JONG;
    std::map<string, std::pair<string, int>> KEY_OF_JAMO;
    std::map<string, string> INV_VOWEL;

    Tables() {
        CHO  = utf8chars(u8"ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ");
        JUNG = utf8chars(u8"ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ");
        JONG = utf8chars(u8" ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ");

        VOWEL_KEYS = {{"K1", u8"ㅣ"}, {"K2", u8"ㆍ"}, {"K3", u8"ㅡ"}};
        CONSONANT_CYCLE = {
            {"K4", {u8"ㄱ", u8"ㅋ", u8"ㄲ"}},
            {"K5", {u8"ㄴ", u8"ㄹ"}},
            {"K6", {u8"ㄷ", u8"ㅌ", u8"ㄸ"}},
            {"K7", {u8"ㅂ", u8"ㅍ", u8"ㅃ"}},
            {"K8", {u8"ㅅ", u8"ㅎ", u8"ㅆ"}},
            {"K9", {u8"ㅈ", u8"ㅊ", u8"ㅉ"}},
            {"K0", {u8"ㅇ", u8"ㅁ"}},
        };
        for (auto& kv : CONSONANT_CYCLE) LONGPRESS[kv.first] = kv.second.back();

        auto V_ = [&](const string& st, std::initializer_list<std::pair<string,string>> tr) {
            std::map<string, string> m;
            vector<std::pair<string,string>> ord;
            for (auto& p : tr) { m[p.first] = p.second; ord.push_back(p); }
            V[st] = m;
            V_ORDER.push_back({st, ord});
            V_ORD[st] = ord;
        };
        V_("",        {{u8"ㅣ",u8"ㅣ"},{u8"ㆍ",u8"ㆍ"},{u8"ㅡ",u8"ㅡ"}});
        V_(u8"ㅣ",    {{u8"ㆍ",u8"ㅏ"}});
        V_(u8"ㆍ",    {{u8"ㅣ",u8"ㅓ"},{u8"ㅡ",u8"ㅗ"},{u8"ㆍ",u8"ㆍㆍ"}});
        V_(u8"ㆍㆍ",  {{u8"ㅣ",u8"ㅕ"},{u8"ㅡ",u8"ㅛ"},{u8"ㆍ",u8"ㆍ"}});
        V_(u8"ㅡ",    {{u8"ㆍ",u8"ㅜ"},{u8"ㅣ",u8"ㅢ"}});
        V_(u8"ㅏ",    {{u8"ㆍ",u8"ㅑ"},{u8"ㅣ",u8"ㅐ"}});
        V_(u8"ㅑ",    {{u8"ㅣ",u8"ㅒ"},{u8"ㆍ",u8"ㅏ"}});
        V_(u8"ㅓ",    {{u8"ㅣ",u8"ㅔ"}});
        V_(u8"ㅕ",    {{u8"ㅣ",u8"ㅖ"}});
        V_(u8"ㅐ",    {{u8"ㆍ",u8"ㅒ"}});
        V_(u8"ㅗ",    {{u8"ㅣ",u8"ㅚ"}});
        V_(u8"ㅚ",    {{u8"ㆍ",u8"ㅘ"}});
        V_(u8"ㅘ",    {{u8"ㅣ",u8"ㅙ"}});
        V_(u8"ㅜ",    {{u8"ㅣ",u8"ㅟ"},{u8"ㆍ",u8"ㅠ"}});
        V_(u8"ㅠ",    {{u8"ㅣ",u8"ㅝ"}});
        V_(u8"ㅝ",    {{u8"ㅣ",u8"ㅞ"}});
        V_(u8"ㅟ",    {{u8"ㆍ",u8"ㅝ"}});
        for (auto s : {u8"ㅛ",u8"ㅒ",u8"ㅔ",u8"ㅖ",u8"ㅙ",u8"ㅞ",u8"ㅢ"}) V_(s, {});

        PENDING = {u8"ㆍ", u8"ㆍㆍ"};
        for (auto& kv : V_ORDER)
            for (auto& tr : kv.second)
                if (!tr.second.empty() && !PREV.count(tr.second)) PREV[tr.second] = kv.first;

        const char* cl[][3] = {
            {u8"ㄱ",u8"ㅅ",u8"ㄳ"},{u8"ㄴ",u8"ㅈ",u8"ㄵ"},{u8"ㄴ",u8"ㅎ",u8"ㄶ"},
            {u8"ㄹ",u8"ㄱ",u8"ㄺ"},{u8"ㄹ",u8"ㅁ",u8"ㄻ"},{u8"ㄹ",u8"ㅂ",u8"ㄼ"},
            {u8"ㄹ",u8"ㅅ",u8"ㄽ"},{u8"ㄹ",u8"ㅌ",u8"ㄾ"},{u8"ㄹ",u8"ㅍ",u8"ㄿ"},
            {u8"ㄹ",u8"ㅎ",u8"ㅀ"},{u8"ㅂ",u8"ㅅ",u8"ㅄ"}};
        for (auto& c : cl) {
            JONG_COMBINE[string(c[0]) + "|" + c[1]] = c[2];
            JONG_SPLIT[c[2]] = {c[0], c[1]};
        }
        for (size_t i = 1; i < JONG.size(); i++) VALID_JONG.insert(JONG[i]);

        for (auto& kv : CONSONANT_CYCLE)
            for (size_t i = 0; i < kv.second.size(); i++)
                if (!KEY_OF_JAMO.count(kv.second[i]))
                    KEY_OF_JAMO[kv.second[i]] = {kv.first, int(i) + 1};
        for (auto& kv : VOWEL_KEYS) INV_VOWEL[kv.second] = kv.first;
    }
};

inline const Tables& T() { static Tables t; return t; }

inline int idxOf(const vector<string>& v, const string& s) {
    auto it = std::find(v.begin(), v.end(), s);
    return it == v.end() ? -1 : int(it - v.begin());
}

inline string compose(const string& cho, const string& jung, const string& jong) {
    const Tables& t = T();
    int j = jong.empty() ? 0 : idxOf(t.JONG, jong);
    if (j < 0) j = 0;
    return fromCodepoint(0xAC00 + (idxOf(t.CHO, cho) * 21 + idxOf(t.JUNG, jung)) * 28 + j);
}

/** @return {초성,중성,종성} / 한글 음절이 아니면 빈 벡터 */
inline vector<string> decompose(const string& ch) {
    unsigned cp = toCodepoint(ch);
    if (cp < 0xAC00 || cp > 0xD7A3) return {};
    unsigned c = cp - 0xAC00;
    const Tables& t = T();
    string jong = t.JONG[c % 28];
    if (jong == " ") jong = "";
    return {t.CHO[c / 588], t.JUNG[(c % 588) / 28], jong};
}

/* ───────── 상태 ───────── */
struct State {
    string cho, vstate, jong;
    string lastKey;          // 빈 문자열 = null
    int tap = 0;
    bool hasSnap = false;
    string snapCho, snapV, snapJong, snapCommitted, snapSlot;
    string committed;
    string slot = "cho";

    string preedit() const {
        const Tables& t = T();
        string v = t.PENDING.count(vstate) ? "" : vstate;
        if (!cho.empty() && !v.empty()) return compose(cho, v, jong);
        if (!cho.empty()) return cho;
        return v;
    }
    string text() const { return committed + preedit(); }
};

inline State newState() { return State(); }

inline State flushS(const State& s) {
    State t = s;
    t.committed = s.committed + s.preedit();
    t.cho.clear(); t.vstate.clear(); t.jong.clear();
    t.lastKey.clear(); t.tap = 0; t.hasSnap = false; t.slot = "cho";
    return t;
}

inline void takeSnap(State& t, const State& base) {
    t.hasSnap = true;
    t.snapCho = base.cho; t.snapV = base.vstate; t.snapJong = base.jong;
    t.snapCommitted = base.committed; t.snapSlot = base.slot;
}
inline State restoreSnap(const State& s) {
    State t = s;
    t.cho = s.snapCho; t.vstate = s.snapV; t.jong = s.snapJong;
    t.committed = s.snapCommitted; t.slot = s.snapSlot;
    return t;
}

inline State attach(const State& s, const string& c) {
    const Tables& tb = T();
    State t;
    if (s.slot == "cho" && s.cho.empty()) { t = s; t.cho = c; t.slot = "cho"; return t; }
    if (s.slot == "jong" && !s.jong.empty()) {
        auto it = tb.JONG_COMBINE.find(s.jong + "|" + c);
        if (it != tb.JONG_COMBINE.end()) { t = s; t.jong = it->second; t.slot = "jong"; return t; }
        t = flushS(s); t.cho = c; t.slot = "cho"; return t;
    }
    if (!s.cho.empty() && !s.vstate.empty() && !tb.PENDING.count(s.vstate)) {
        if (tb.VALID_JONG.count(c)) { t = s; t.jong = c; t.slot = "jong"; return t; }
        t = flushS(s); t.cho = c; t.slot = "cho"; return t;
    }
    t = flushS(s); t.cho = c; t.slot = "cho"; return t;
}

inline State backspace(const State& s);

inline State press(const State& s0, const string& key) {
    const Tables& tb = T();
    State s = s0, t;

    // 모음
    auto vk = tb.VOWEL_KEYS.find(key);
    if (vk != tb.VOWEL_KEYS.end()) {
        const string& jamo = vk->second;
        if (!s.jong.empty()) {                       // 연음
            string moved, rest;
            auto sp = tb.JONG_SPLIT.find(s.jong);
            if (sp != tb.JONG_SPLIT.end()) { moved = sp->second.second; rest = sp->second.first; }
            else { moved = s.jong; rest = ""; }
            State b = s; b.jong = rest;
            b = flushS(b);
            b.cho = moved; b.slot = "jung";
            s = b;
        }
        string nxt;
        auto vi = tb.V.find(s.vstate);
        if (vi != tb.V.end()) { auto ji = vi->second.find(jamo); if (ji != vi->second.end()) nxt = ji->second; }
        if (nxt.empty()) { s = flushS(s); nxt = tb.V.at("").at(jamo); }
        t = s; t.vstate = nxt; t.lastKey.clear(); t.tap = 0; t.hasSnap = false; t.slot = "jung";
        return t;
    }

    // 자음 멀티탭
    auto ck = tb.CONSONANT_CYCLE.find(key);
    if (ck != tb.CONSONANT_CYCLE.end()) {
        const vector<string>& cyc = ck->second;
        int tap; State base;
        if (s.lastKey == key && s.hasSnap) { tap = (s.tap + 1) % int(cyc.size()); base = restoreSnap(s); }
        else { tap = 0; base = s; }
        t = attach(base, cyc[tap]);
        takeSnap(t, base);
        t.lastKey = key; t.tap = tap;
        return t;
    }

    // 롱프레스
    if (key.rfind("LONG_", 0) == 0) {
        string k = key.substr(5);
        auto li = tb.LONGPRESS.find(k);
        if (li == tb.LONGPRESS.end()) return s;
        t = attach(s, li->second);
        t.lastKey.clear(); t.tap = 0; t.hasSnap = false;
        return t;
    }

    if (key == "KRIGHT" || key == "KLEFT") {
        if (!s.preedit().empty()) return flushS(s);
        t = s; t.lastKey.clear(); t.tap = 0; t.hasSnap = false; return t;
    }
    if (key == "TIMEOUT") { t = s; t.lastKey.clear(); t.tap = 0; t.hasSnap = false; return t; }
    if (key == "SPACE")  { t = flushS(s); t.committed += " ";  return t; }
    if (key == "ENTER")  { t = flushS(s); t.committed += "\n"; return t; }
    if (key == "COMMIT") return flushS(s);
    if (key == "BACK")   return backspace(s);
    if (key.rfind("CHAR_", 0) == 0) { t = flushS(s); t.committed += key.substr(5); return t; }
    return s;
}

inline State backspace(const State& s) {
    const Tables& tb = T();
    State t;
    if (!s.jong.empty()) {
        t = s;
        auto sp = tb.JONG_SPLIT.find(s.jong);
        t.jong = (sp != tb.JONG_SPLIT.end()) ? sp->second.first : "";
        if (t.jong.empty()) t.slot = "jung";
        t.lastKey.clear(); t.tap = 0; t.hasSnap = false;
        return t;
    }
    if (!s.vstate.empty()) {
        string prev;
        auto pi = tb.PREV.find(s.vstate);
        if (pi != tb.PREV.end()) prev = pi->second;
        t = s; t.vstate = prev; t.slot = prev.empty() ? "cho" : "jung";
        t.lastKey.clear(); t.tap = 0; t.hasSnap = false;
        return t;
    }
    if (!s.cho.empty()) {
        t = s; t.cho.clear(); t.slot = "cho";
        t.lastKey.clear(); t.tap = 0; t.hasSnap = false;
        return t;
    }
    if (!s.committed.empty()) {
        t = s;
        auto cs = utf8chars(s.committed);
        cs.pop_back();
        t.committed.clear();
        for (auto& c : cs) t.committed += c;
        return t;
    }
    return s;
}

inline string typeKeys(const vector<string>& keys) {
    State s = newState();
    for (auto& k : keys) s = press(s, k);
    return flushS(s).committed;
}

/* ───────── 역방향 ───────── */
inline vector<string> vowelSeq(const string& v) {
    const Tables& tb = T();
    static std::map<string, vector<string>> cache;
    auto it = cache.find(v);
    if (it != cache.end()) return it->second;
    std::deque<std::pair<string, vector<string>>> q;
    q.push_back({"", {}});
    std::set<string> seen{""};
    while (!q.empty()) {
        auto cur = q.front(); q.pop_front();
        if (cur.first == v) { cache[v] = cur.second; return cur.second; }
        auto vi = tb.V_ORD.find(cur.first);
        if (vi == tb.V_ORD.end()) continue;
        for (auto& tr : vi->second) {
            if (tr.second.empty() || seen.count(tr.second)) continue;
            seen.insert(tr.second);
            auto np = cur.second;
            np.push_back(tb.INV_VOWEL.at(tr.first));
            q.push_back({tr.second, np});
        }
    }
    return {};
}

inline vector<string> encode(const string& str, bool useLongpress) {
    const Tables& tb = T();
    vector<string> keys;
    string lastKey;
    for (auto& ch : utf8chars(str)) {
        if (ch == " ")  { keys.push_back("SPACE"); lastKey.clear(); continue; }
        if (ch == "\n") { keys.push_back("ENTER"); lastKey.clear(); continue; }
        auto d = decompose(ch);
        if (d.empty()) { keys.push_back("CHAR_" + ch); lastKey.clear(); continue; }

        auto cons = [&](const string& jamo, vector<string>& out) -> string {
            auto kt = tb.KEY_OF_JAMO.at(jamo);
            if (useLongpress && tb.LONGPRESS.at(kt.first) == jamo) {
                out.push_back("LONG_" + kt.first);
                return "";
            }
            for (int i = 0; i < kt.second; i++) out.push_back(kt.first);
            return kt.first;
        };

        vector<string> sy;
        cons(d[0], sy);
        for (auto& k : vowelSeq(d[1])) sy.push_back(k);
        string syLast;
        if (!d[2].empty()) {
            vector<string> parts;
            auto sp = tb.JONG_SPLIT.find(d[2]);
            if (sp != tb.JONG_SPLIT.end()) { parts = {sp->second.first, sp->second.second}; }
            else parts = {d[2]};
            for (auto& p : parts) {
                vector<string> ps;
                string pk = cons(p, ps);
                if (!pk.empty() && pk == syLast) sy.push_back("KRIGHT");
                for (auto& k : ps) sy.push_back(k);
                syLast = pk;
            }
        }
        if (!sy.empty() && sy[0] == lastKey) keys.push_back("KRIGHT");
        for (auto& k : sy) keys.push_back(k);
        lastKey = syLast;
    }
    return keys;
}

} // namespace cuime
