// 천지인 IME 셸 — 프레임워크 비의존 상태 머신
// Windows TSF · Linux IBus · macOS IMK 가 이 파일을 공유한다.
//
// 코어(cheonjiin.hpp)는 시간을 모른다. 이 셸이 타이머 만료를 timeout() 으로 받고,
// 漢 키 판정 · 레이아웃 전환 · M1 전송 가드를 담당한다.
// docs/00-확정스펙.md §4, §5, §7, §8.1
#pragma once
#include "cheonjiin.hpp"
#include <string>
#include <vector>
#include <map>
#include <cctype>

namespace cuime {

enum class Layout { Hangul, English, Number };

/** IBus/TSF/IMK 공용 셸 상태 머신 — 프레임워크 비의존 */
class Shell {
public:
    struct Config {
        int multitapMs  = 800;
        int longpressMs = 300;
        int sendGuardMs = 300;
        bool enterIsSend = false;
    } cfg;

    State core = newState();
    Layout layout = Layout::Hangul;
    bool shift = false;

    // 漢 코드 판정
    bool hjDown = false, hjConsumed = false;

    // 후보 / 팔레트
    vector<string> candidates;
    int candIndex = 0;
    string candTarget;
    bool paletteOpen = false;
    int palettePage = 0;

    // 영어 멀티탭
    string enKey;
    int enTap = 0;

    // M1 가드
    long long lastBackAt = 0;
    string warning;

    /* 현재 시각은 외부에서 주입 (코어 순수성 유지) */
    long long nowMs = 0;

    static const std::map<string, vector<string>>& english() {
        static std::map<string, vector<string>> m = {
            {"K1", {".", ",", "?", "!", "'"}},
            {"K2", {"a","b","c"}}, {"K3", {"d","e","f"}}, {"K4", {"g","h","i"}},
            {"K5", {"j","k","l"}}, {"K6", {"m","n","o"}}, {"K7", {"p","q","r","s"}},
            {"K8", {"t","u","v"}}, {"K9", {"w","x","y","z"}},
        };
        return m;
    }
    static const std::map<string, string>& numbers() {
        static std::map<string, string> m = {
            {"K0","0"},{"K1","1"},{"K2","2"},{"K3","3"},{"K4","4"},
            {"K5","5"},{"K6","6"},{"K7","7"},{"K8","8"},{"K9","9"}};
        return m;
    }
    static const vector<vector<string>>& symbols() {
        static vector<vector<string>> s = {
            {".",",","?","!","~","…","'","\"","(",")",":",";","-","/","@","#"},
            {"+","-","×","÷","=","±","%","°","℃","₩","$","€","¥","№","※","★"},
            {"[","]","{","}","〈","〉","《","》","「","」","←","→","↑","↓","↔","⇒"},
        };
        return s;
    }

    string text() const { return core.text(); }
    string preedit() const { return core.preedit(); }

    void tap(const string& k) {
        warning.clear();
        if (paletteOpen) return;
        if (!candidates.empty()) candidates.clear();
        if (layout == Layout::Hangul) {
            core = press(core, k);
        } else if (layout == Layout::English) {
            typeEnglish(k, false);
        } else {
            auto it = numbers().find(k);
            if (it != numbers().end()) { enKey.clear(); enTap = 0; core = press(core, "CHAR_" + it->second); }
        }
    }

    /** 롱프레스 = 순환열 마지막 (docs/00 §3.2) */
    void longPress(const string& k) {
        warning.clear();
        if (paletteOpen) return;
        if (layout == Layout::Hangul) {
            if (T().CONSONANT_CYCLE.count(k)) core = press(core, "LONG_" + k);
        } else if (layout == Layout::English) {
            typeEnglish(k, true);
        }
    }

    /** 멀티탭 타임아웃 — 셸 타이머가 만료되면 호출 */
    void timeout() { core = press(core, "TIMEOUT"); enKey.clear(); enTap = 0; }

    void hanjaDown() { hjDown = true; hjConsumed = false; }

    /** 漢 키는 뗄 때 판정한다 (docs/00 §5) */
    void hanjaUp(const string& selection, bool selIsHangul,
                 const vector<string>& dictHit) {
        bool consumed = hjConsumed;
        hjDown = false; hjConsumed = false;
        if (consumed) return;                       // 코드로 소비됨
        if (!candidates.empty()) { candidates.clear(); return; }
        if (paletteOpen) { paletteOpen = false; return; }
        core = press(core, "TIMEOUT");
        if (!selection.empty() && selIsHangul && !dictHit.empty()) {
            candidates = dictHit; candIndex = 0; candTarget = selection;
            paletteOpen = false;
        } else {
            paletteOpen = true; palettePage = 0;
        }
    }

    void hanjaLongPress() {
        hjConsumed = true; candidates.clear();
        paletteOpen = true; palettePage = 0;
    }

    /** 방향키 — 문맥 의존 4중 동작 (docs/00 §4) */
    void arrow(int dir) {
        warning.clear();
        if (hjDown) {                                // (1) 코드 → 레이아웃
            if (!hjConsumed) {
                int n = 3;
                int i = ((int)layout + dir % n + n) % n;
                layout = (Layout)i;
                hjConsumed = true;
                candidates.clear(); paletteOpen = false;
                enKey.clear(); enTap = 0;
                core = press(core, "COMMIT");
            }
            return;
        }
        if (!candidates.empty()) {                   // (2) 후보 탐색
            int n = (int)candidates.size();
            candIndex = (candIndex + dir % n + n) % n;
            return;
        }
        if (paletteOpen) {                           // (3) 팔레트 페이지
            int n = (int)symbols().size();
            palettePage = (palettePage + dir % n + n) % n;
            return;
        }
        enKey.clear(); enTap = 0;                    // (4) 확정 / 커서
        core = press(core, dir > 0 ? "KRIGHT" : "KLEFT");
    }

    void backspace() {
        warning.clear();
        enKey.clear(); enTap = 0;
        if (!candidates.empty()) { candidates.clear(); return; }
        if (paletteOpen) { paletteOpen = false; return; }
        lastBackAt = nowMs;                          // M1 가드 기록
        core = backspaceS(core);
    }

    void space() {
        enKey.clear(); enTap = 0;
        if (!candidates.empty()) { applyCandidate(); return; }
        core = press(core, "SPACE");
    }

    /** 엔터 — M1 전송 가드 (docs/00 §7). @return 전송 수행 여부 */
    bool enter() {
        enKey.clear(); enTap = 0;
        if (!candidates.empty()) { applyCandidate(); return false; }
        if (paletteOpen) { paletteOpen = false; return false; }
        long long since = nowMs - lastBackAt;
        if (cfg.enterIsSend && lastBackAt > 0 && since < cfg.sendGuardMs) {
            lastBackAt = 0;
            warning = "⌫ 직후 전송 차단 (M1)";
            return false;
        }
        core = press(core, "ENTER");
        return true;
    }

    void pickSymbol(const string& s, bool keepOpen) {
        enKey.clear(); enTap = 0;
        core = press(core, "CHAR_" + s);
        if (!keepOpen) paletteOpen = false;
    }

    void applyCandidate() {
        if (candidates.empty()) return;
        string pick = candidates[candIndex];
        size_t n = utf8chars(candTarget).size();
        core = flushS(core);
        auto cs = utf8chars(core.committed);
        string rebuilt;
        for (size_t i = 0; i + n < cs.size() + 0 || (n <= cs.size() && i < cs.size() - n); i++)
            rebuilt += cs[i];
        core.committed = rebuilt + pick;
        candidates.clear(); candIndex = 0; candTarget.clear();
    }

private:
    void typeEnglish(const string& k, bool isLong) {
        auto it = english().find(k);
        if (it == english().end()) { if (k == "K0") shift = !shift; return; }
        const vector<string>& cyc = it->second;
        string ch;
        if (isLong) { ch = cyc.back(); enKey.clear(); enTap = 0; }
        else {
            if (k == enKey) { core = press(core, "BACK"); enTap = (enTap + 1) % (int)cyc.size(); }
            else { enKey = k; enTap = 0; }
            ch = cyc[enTap];
        }
        if (shift) { for (auto& c : ch) c = toupper(c); shift = false; }
        core = press(core, "CHAR_" + ch);
    }

    static State backspaceS(const State& s) { return cuime::backspace(s); }
};

} // namespace cuime
