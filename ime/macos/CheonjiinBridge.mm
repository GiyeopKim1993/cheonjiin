// 천지인 macOS IME — InputMethodKit 브리지
// docs/00-확정스펙.md §9
//
// 조합 로직은 shell.hpp(공용 셸) + cheonjiin.hpp(코어)를 그대로 재사용한다.
// 이 파일은 IMK <-> C++ 셸 사이의 얇은 어댑터일 뿐이다.
//
// 빌드:
//   clang++ -std=c++17 -fobjc-arc -I../native -c CheonjiinBridge.mm
//   (앱 번들: InputMethodKit.framework, Cocoa.framework 링크)

#include "shell.hpp"
#include "hanja.hpp"

#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────
// C++ 측: IMK 가 호출할 순수 인터페이스
// Objective-C++ 파일이지만 이 부분은 Foundation 없이도 컴파일된다.
// ─────────────────────────────────────────────────────────────
namespace cuime {
namespace imk {

/** macOS 가상 키코드 -> 코어 키 (docs/03 §2.2) */
enum MacKey : unsigned short {
    kVK_Delete      = 0x33,
    kVK_Return      = 0x24,
    kVK_Space       = 0x31,
    kVK_LeftArrow   = 0x7B,
    kVK_RightArrow  = 0x7C,
    // 숫자패드
    kVK_ANSI_Keypad0 = 0x52, kVK_ANSI_Keypad1 = 0x53, kVK_ANSI_Keypad2 = 0x54,
    kVK_ANSI_Keypad3 = 0x55, kVK_ANSI_Keypad4 = 0x56, kVK_ANSI_Keypad5 = 0x57,
    kVK_ANSI_Keypad6 = 0x58, kVK_ANSI_Keypad7 = 0x59, kVK_ANSI_Keypad8 = 0x5B,
    kVK_ANSI_Keypad9 = 0x5C,
    // 상단 숫자열
    kVK_ANSI_1 = 0x12, kVK_ANSI_2 = 0x13, kVK_ANSI_3 = 0x14, kVK_ANSI_4 = 0x15,
    kVK_ANSI_5 = 0x17, kVK_ANSI_6 = 0x16, kVK_ANSI_7 = 0x1A, kVK_ANSI_8 = 0x1C,
    kVK_ANSI_9 = 0x19, kVK_ANSI_0 = 0x1D,
    kVK_ANSI_Grave = 0x32,          // ` -> 漢 대체
};

enum class PadMode { Phone, NumPad };

inline std::string mapKeyCode(unsigned short kc, PadMode mode) {
    switch (kc) {
        case kVK_ANSI_1: return "K1";  case kVK_ANSI_2: return "K2";
        case kVK_ANSI_3: return "K3";  case kVK_ANSI_4: return "K4";
        case kVK_ANSI_5: return "K5";  case kVK_ANSI_6: return "K6";
        case kVK_ANSI_7: return "K7";  case kVK_ANSI_8: return "K8";
        case kVK_ANSI_9: return "K9";  case kVK_ANSI_0: return "K0";
        case kVK_Delete:     return "BACK";
        case kVK_Return:     return "ENTER";
        case kVK_Space:      return "SPACE";
        case kVK_LeftArrow:  return "LEFT";
        case kVK_RightArrow: return "RIGHT";
        case kVK_ANSI_Grave: return "HANJA";
        default: break;
    }
    // 숫자패드: 전화기 논리 매핑이 기본 (모바일 근육 기억 전이 우선)
    int n = -1;
    switch (kc) {
        case kVK_ANSI_Keypad0: n = 0; break;  case kVK_ANSI_Keypad1: n = 1; break;
        case kVK_ANSI_Keypad2: n = 2; break;  case kVK_ANSI_Keypad3: n = 3; break;
        case kVK_ANSI_Keypad4: n = 4; break;  case kVK_ANSI_Keypad5: n = 5; break;
        case kVK_ANSI_Keypad6: n = 6; break;  case kVK_ANSI_Keypad7: n = 7; break;
        case kVK_ANSI_Keypad8: n = 8; break;  case kVK_ANSI_Keypad9: n = 9; break;
        default: return "";
    }
    if (mode == PadMode::NumPad) return "K" + std::to_string(n);
    int phone;
    switch (n) {
        case 7: phone = 1; break; case 8: phone = 2; break; case 9: phone = 3; break;
        case 4: phone = 4; break; case 5: phone = 5; break; case 6: phone = 6; break;
        case 1: phone = 7; break; case 2: phone = 8; break; case 3: phone = 9; break;
        default: phone = 0; break;
    }
    return "K" + std::to_string(phone);
}

/**
 * IMK 컨트롤러가 보유하는 조합 호스트.
 * IMKInputController 는 이 객체에 키를 전달하고,
 * markedText()/commitText() 결과를 클라이언트에 반영한다.
 */
class Host {
public:
    Shell shell;
    PadMode padMode = PadMode::Phone;
    std::string pendingCommit;
    std::string marked;
    std::string selection;          // 클라이언트에서 읽어온 선택 영역

    /** @return true = 우리가 소비 (IMK 의 handleEvent 반환값) */
    bool onKeyDown(unsigned short kc) {
        std::string k = mapKeyCode(kc, padMode);
        if (k.empty()) return false;
        if (k == "HANJA") { shell.hanjaDown(); return true; }
        if (k == "LEFT")  { shell.arrow(-1); sync(); return true; }
        if (k == "RIGHT") { shell.arrow(1);  sync(); return true; }
        if (k == "BACK")  { shell.backspace(); sync(); return true; }
        if (k == "SPACE") { shell.space(); sync(); return true; }
        if (k == "ENTER") { bool sent = shell.enter(); sync(); return !sent; }
        shell.tap(k); sync();
        return true;
    }

    bool onKeyUp(unsigned short kc) {
        if (mapKeyCode(kc, padMode) != "HANJA") return false;
        bool han = isHangul(selection);
        shell.hanjaUp(selection, han,
                      han ? hanja::lookup(selection) : std::vector<std::string>{});
        sync();
        return true;
    }

    /** 멀티탭 타임아웃 — NSTimer 만료 시 호출 */
    void onTimeout() { shell.timeout(); sync(); }

    void onLongPress(unsigned short kc) {
        std::string k = mapKeyCode(kc, padMode);
        if (k.size() == 2 && k[0] == 'K') { shell.longPress(k); sync(); }
        else if (k == "HANJA") { shell.hanjaLongPress(); sync(); }
    }

    void sync() {
        if (!shell.core.committed.empty()) {
            pendingCommit += shell.core.committed;
            shell.core.committed.clear();
        }
        marked = shell.core.preedit();
    }

    std::string takeCommit() { std::string s = pendingCommit; pendingCommit.clear(); return s; }
    bool hasMarked() const { return !marked.empty(); }

    /** 조합 강제 종료 (commitComposition:) */
    void commitAll() {
        shell.core = press(shell.core, "COMMIT");
        sync();
    }

    static bool isHangul(const std::string& s) {
        if (s.empty()) return false;
        for (auto& ch : utf8chars(s)) {
            unsigned cp = toCodepoint(ch);
            if (cp < 0xAC00 || cp > 0xD7A3) return false;
        }
        return true;
    }
};

} // namespace imk
} // namespace cuime


#if defined(__OBJC__) && !defined(CUIME_IMK_NO_OBJC)
/* ───────── IMKInputController 구현 (macOS 빌드 전용) ─────────
 *
 * #import <InputMethodKit/InputMethodKit.h>
 *
 * @interface CheonjiinController : IMKInputController {
 *     cuime::imk::Host *_host;
 *     NSTimer *_multitapTimer;
 * }
 * @end
 *
 * @implementation CheonjiinController
 *
 * - (BOOL)handleEvent:(NSEvent *)event client:(id)sender {
 *     if (event.type == NSEventTypeKeyDown) {
 *         // 선택 영역을 셸에 전달 (한자 변환 대상)
 *         NSRange sel = [sender selectedRange];
 *         if (sel.length > 0) {
 *             NSString *s = [sender attributedSubstringFromRange:sel].string;
 *             _host->selection = std::string(s.UTF8String);
 *         } else {
 *             _host->selection = _host->shell.core.preedit();
 *         }
 *         BOOL eaten = _host->onKeyDown(event.keyCode);
 *         if (eaten) { [self armMultitap]; [self render:sender]; }
 *         return eaten;
 *     }
 *     if (event.type == NSEventTypeKeyUp) {
 *         BOOL eaten = _host->onKeyUp(event.keyCode);
 *         if (eaten) [self render:sender];
 *         return eaten;
 *     }
 *     return NO;
 * }
 *
 * - (void)render:(id)sender {
 *     std::string commit = _host->takeCommit();
 *     if (!commit.empty())
 *         [sender insertText:@(commit.c_str())
 *          replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
 *
 *     if (_host->hasMarked()) {
 *         NSString *m = @(_host->marked.c_str());
 *         NSDictionary *attr = [self markForStyle:kTSMHiliteConvertedText
 *                                        atRange:NSMakeRange(0, m.length)];
 *         NSAttributedString *as = [[NSAttributedString alloc] initWithString:m
 *                                                                  attributes:attr];
 *         [sender setMarkedText:as
 *                 selectionRange:NSMakeRange(m.length, 0)
 *               replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
 *     } else {
 *         [sender setMarkedText:@""
 *                 selectionRange:NSMakeRange(0, 0)
 *               replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
 *     }
 * }
 *
 * // 멀티탭 타이머는 셸이 소유 (docs/00 §8.1 — 코어는 시간을 모른다)
 * - (void)armMultitap {
 *     [_multitapTimer invalidate];
 *     NSTimeInterval t = _host->shell.cfg.multitapMs / 1000.0;
 *     _multitapTimer = [NSTimer scheduledTimerWithTimeInterval:t repeats:NO
 *         block:^(NSTimer *_) {
 *             _host->onTimeout();
 *             [self render:[self client]];
 *         }];
 * }
 *
 * - (void)commitComposition:(id)sender {
 *     [_multitapTimer invalidate]; _multitapTimer = nil;
 *     _host->commitAll();
 *     [self render:sender];
 * }
 *
 * - (void)deactivateServer:(id)sender { [self commitComposition:sender]; }
 *
 * @end
 */
#endif


#ifdef CUIME_IMK_TEST
/* ───────── 브리지 검증 (Linux/clang 공용, Objective-C 불필요) ───────── */
#include <iostream>
using namespace cuime;
using namespace cuime::imk;
static int fails = 0;
static void chk(const std::string& n, const std::string& g, const std::string& e) {
    if (g != e) { std::cout << "❌ " << n << " got=" << g << " exp=" << e << "\n"; fails++; }
    else std::cout << "✅ " << n << "\n";
}
static void chki(const std::string& n, long g, long e) { chk(n, std::to_string(g), std::to_string(e)); }

int main() {
    // 키 매핑
    chk("숫자열 1", mapKeyCode(kVK_ANSI_1, PadMode::Phone), "K1");
    chk("숫자열 0", mapKeyCode(kVK_ANSI_0, PadMode::Phone), "K0");
    chk("Keypad7 -> K1 (전화기)", mapKeyCode(kVK_ANSI_Keypad7, PadMode::Phone), "K1");
    chk("Keypad1 -> K7 (전화기)", mapKeyCode(kVK_ANSI_Keypad1, PadMode::Phone), "K7");
    chk("Keypad5 -> K5", mapKeyCode(kVK_ANSI_Keypad5, PadMode::Phone), "K5");
    chk("Keypad7 -> K7 (물리)", mapKeyCode(kVK_ANSI_Keypad7, PadMode::NumPad), "K7");
    chk("Delete", mapKeyCode(kVK_Delete, PadMode::Phone), "BACK");
    chk("Return", mapKeyCode(kVK_Return, PadMode::Phone), "ENTER");
    chk("RightArrow", mapKeyCode(kVK_RightArrow, PadMode::Phone), "RIGHT");
    chk("미매핑", mapKeyCode(0x00, PadMode::Phone), "");

    // 조합: 나무위키
    { Host h;
      unsigned short seq[] = {kVK_ANSI_5,kVK_ANSI_1,kVK_ANSI_2,kVK_ANSI_0,kVK_ANSI_0,
                              kVK_ANSI_3,kVK_ANSI_2,kVK_ANSI_0,kVK_ANSI_3,kVK_ANSI_2,
                              kVK_ANSI_1,kVK_ANSI_4,kVK_ANSI_4,kVK_ANSI_1};
      for (auto k : seq) h.onKeyDown(k);
      h.commitAll();
      chk("IMK 조합 나무위키", h.pendingCommit + h.marked, u8"나무위키"); }

    // marked / commit 분리
    { Host h;
      h.onKeyDown(kVK_ANSI_4);
      chki("marked 있음", h.hasMarked() ? 1 : 0, 1);
      chk("marked ㄱ", h.marked, u8"ㄱ");
      h.onKeyDown(kVK_ANSI_1); h.onKeyDown(kVK_ANSI_2);
      chk("marked 가", h.marked, u8"가");
      h.onKeyDown(kVK_Space);
      chk("공백 커밋", h.takeCommit(), u8"가 ");
      chki("marked 해제", h.hasMarked() ? 1 : 0, 0); }

    // 한자 변환 (실제 사전)
    { Host h; h.selection = u8"대한민국";
      h.onKeyDown(kVK_ANSI_Grave); h.onKeyUp(kVK_ANSI_Grave);
      chki("한자 후보 수", (long)h.shell.candidates.size(), 1);
      chk("한자 후보", h.shell.candidates[0], u8"大韓民國"); }

    // 코드 -> 레이아웃 전환
    { Host h;
      h.onKeyDown(kVK_ANSI_Grave);
      h.onKeyDown(kVK_RightArrow);
      h.onKeyUp(kVK_ANSI_Grave);
      chki("코드 -> 영어", (long)h.shell.layout, 1);
      chki("팔레트 미발생", h.shell.paletteOpen ? 1 : 0, 0); }

    // M1 전송 가드
    { Host h; h.shell.cfg.enterIsSend = true; h.shell.nowMs = 1000;
      h.onKeyDown(kVK_ANSI_4); h.onKeyDown(kVK_ANSI_1); h.onKeyDown(kVK_ANSI_2);
      h.onKeyDown(kVK_Delete);
      h.shell.nowMs = 1100;
      chki("M1 차단(소비)", h.onKeyDown(kVK_Return) ? 1 : 0, 1);
      chki("M1 경고", h.shell.warning.empty() ? 0 : 1, 1); }

    std::cout << "\n" << (fails == 0
        ? "✅ macOS IMK 브리지 전 항목 통과 (키매핑·조합·한자·코드·M1)"
        : "❌ 실패") << "\n";
    return fails ? 1 : 0;
}
#endif
