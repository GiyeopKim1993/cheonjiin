// 천지인 Windows IME — TSF (Text Services Framework) 텍스트 서비스
// docs/00-확정스펙.md §9
//
// 구조:
//   CheonjiinTextService : ITfTextInputProcessorEx, ITfThreadMgrEventSink,
//                          ITfKeyEventSink, ITfCompositionSink
//   조합 로직은 cheonjiin.hpp(코어) + ibus_engine.cpp 의 Shell(셸)을 그대로 재사용한다.
//   TSF 계층은 preedit 표시 · 커밋 · 키 매핑만 담당한다.
//
// 빌드 (Windows / MSVC):
//   cl /std:c++17 /LD /EHsc tsf_textservice.cpp /link ole32.lib oleaut32.lib advapi32.lib
//   regsvr32 cuime.dll
//
// 이 저장소(Linux)에서는 CUIME_TSF_STUB 로 Windows 헤더 없이
// 키 매핑·셸 연동 로직만 컴파일·검증한다.

#include <string>
#include <vector>

#include "shell.hpp"
#include "hanja.hpp"

#ifdef CUIME_TSF_STUB
  // ── Linux 검증용 최소 스텁 ──
  typedef unsigned int UINT;
  typedef unsigned short WCHAR;
  typedef unsigned long DWORD;
  typedef int BOOL;
  typedef long HRESULT;
  #define S_OK 0
  #define TRUE 1
  #define FALSE 0
  #define VK_BACK    0x08
  #define VK_RETURN  0x0D
  #define VK_SPACE   0x20
  #define VK_LEFT    0x25
  #define VK_RIGHT   0x27
  #define VK_NUMPAD0 0x60
  #define VK_NUMPAD1 0x61
  #define VK_NUMPAD9 0x69
  #define VK_MULTIPLY 0x6A
  #define VK_ADD      0x6B
  #define VK_HANJA   0x19
  #define VK_OEM_3   0xC0
#else
  #include <windows.h>
  #include <msctf.h>
  #include <olectl.h>
#endif

namespace cuime {
namespace tsf {

/* ───────── 키 매핑 (docs/03 §2.2) ─────────
 * 데스크톱에서는 물리 숫자패드를 1순위로 한다.
 * NumPad 는 위에서부터 789/456/123/0 이고 전화기는 123/456/789/0 이므로
 * 상하가 뒤집혀 있다. 기본값은 "전화기 배열(논리 매핑)" — 모바일 근육 기억 전이 우선.
 */
enum class PadMode { Phone, NumPad };

/** 가상키 -> 코어 키. 매핑 없으면 빈 문자열 */
inline std::string mapVirtualKey(UINT vk, PadMode mode) {
    // 상단 숫자열 '0'~'9' (0x30~0x39)
    if (vk >= 0x30 && vk <= 0x39) {
        int d = int(vk - 0x30);
        return "K" + std::to_string(d);
    }
    // 숫자패드
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD0 + 9) {
        int n = int(vk - VK_NUMPAD0);            // 물리 각인 숫자
        if (mode == PadMode::NumPad) return "K" + std::to_string(n);
        // 전화기 논리 매핑: NumPad 7,8,9 -> 1,2,3 / 1,2,3 -> 7,8,9
        int phone;
        switch (n) {
            case 7: phone = 1; break;  case 8: phone = 2; break;  case 9: phone = 3; break;
            case 4: phone = 4; break;  case 5: phone = 5; break;  case 6: phone = 6; break;
            case 1: phone = 7; break;  case 2: phone = 8; break;  case 3: phone = 9; break;
            default: phone = 0; break;
        }
        return "K" + std::to_string(phone);
    }
    switch (vk) {
        case VK_BACK:   return "BACK";
        case VK_RETURN: return "ENTER";
        case VK_SPACE:  return "SPACE";
        case VK_LEFT:   return "LEFT";
        case VK_RIGHT:  return "RIGHT";
        case VK_HANJA:  return "HANJA";      // 한자 키
        case VK_OEM_3:  return "HANJA";      // ` 키를 漢 대체로
        default: return "";
    }
}

/* ───────── UTF-8 <-> UTF-16 ───────── */
inline std::vector<WCHAR> toUtf16(const std::string& s) {
    std::vector<WCHAR> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        unsigned cp; size_t len;
        if (c < 0x80)      { cp = c;               len = 1; }
        else if (c < 0xE0) { cp = c & 0x1F;        len = 2; }
        else if (c < 0xF0) { cp = c & 0x0F;        len = 3; }
        else               { cp = c & 0x07;        len = 4; }
        for (size_t k = 1; k < len && i + k < s.size(); k++)
            cp = (cp << 6) | (s[i + k] & 0x3F);
        i += len;
        if (cp >= 0x10000) {                       // 서로게이트 쌍
            cp -= 0x10000;
            out.push_back(WCHAR(0xD800 + (cp >> 10)));
            out.push_back(WCHAR(0xDC00 + (cp & 0x3FF)));
        } else {
            out.push_back(WCHAR(cp));
        }
    }
    return out;
}

/**
 * TSF 텍스트 서비스의 조합 상태 관리부.
 * ITfComposition 수명과 preedit/커밋 타이밍을 다룬다.
 * (COM 인터페이스 구현은 Windows 빌드에서만 컴파일)
 */
class CompositionHost {
public:
    Shell shell;                 // ibus_engine.cpp 의 프레임워크 비의존 셸
    PadMode padMode = PadMode::Phone;

    bool compositionActive = false;
    std::string lastPreedit;     // TSF 에 마지막으로 넘긴 조합 문자열
    std::string pendingCommit;   // 이번 턴에 확정할 문자열

    /** 키 입력 처리. @return true = 우리가 소비함 */
    bool onKeyDown(UINT vk, bool /*shiftDown*/) {
        std::string k = mapVirtualKey(vk, padMode);
        if (k.empty()) return false;

        if (k == "HANJA") { shell.hanjaDown(); return true; }
        if (k == "LEFT")  { shell.arrow(-1); sync(); return true; }
        if (k == "RIGHT") { shell.arrow(1);  sync(); return true; }
        if (k == "BACK")  { shell.backspace(); sync(); return true; }
        if (k == "SPACE") { shell.space(); sync(); return true; }
        if (k == "ENTER") {
            bool send = shell.enter();
            sync();
            return !send;        // 전송이면 앱에 넘김(가드로 막혔으면 소비)
        }
        shell.tap(k);
        sync();
        return true;
    }

    bool onKeyUp(UINT vk) {
        std::string k = mapVirtualKey(vk, padMode);
        if (k != "HANJA") return false;
        std::string sel = getSelection();
        bool han = isHangul(sel);
        shell.hanjaUp(sel, han, han ? hanja::lookup(sel) : std::vector<std::string>{});
        sync();
        return true;
    }

    /** 멀티탭 타임아웃 — 셸 타이머(SetTimer)가 만료되면 호출 */
    void onTimeout() { shell.timeout(); sync(); }

    /** 롱프레스 — 키 반복 감지 타이머에서 호출 */
    void onLongPress(UINT vk) {
        std::string k = mapVirtualKey(vk, padMode);
        if (k.size() == 2 && k[0] == 'K') { shell.longPress(k); sync(); }
        else if (k == "HANJA") { shell.hanjaLongPress(); sync(); }
    }

    /** 코어 상태 -> TSF 로 넘길 preedit/commit 계산.
     *  편집 세션(DoEditSession)이 비동기로 지연될 수 있으므로 커밋은 누적한다. */
    void sync() {
        if (!shell.core.committed.empty()) {
            pendingCommit += shell.core.committed;     // append (덮어쓰기 금지)
            shell.core.committed.clear();
        }
        lastPreedit = shell.core.preedit();
    }

    /** 편집 세션에서 커밋을 소비한 뒤 호출 */
    std::string takeCommit() { std::string s = pendingCommit; pendingCommit.clear(); return s; }

    bool needsComposition() const { return !lastPreedit.empty(); }

    /* ── 아래 두 함수는 Windows 빌드에서 TSF API 로 대체 ── */
    virtual std::string getSelection() const { return selectionForTest; }
    std::string selectionForTest;

    static bool isHangul(const std::string& s) {
        if (s.empty()) return false;
        for (auto& ch : utf8chars(s)) {
            unsigned cp = toCodepoint(ch);
            if (cp < 0xAC00 || cp > 0xD7A3) return false;
        }
        return true;
    }
    virtual ~CompositionHost() {}
};

} // namespace tsf
} // namespace cuime

#ifndef CUIME_TSF_STUB
/* COM/TSF 실제 구현. 별도 파일로 분리해 stub 빌드와 섞이지 않게 한다. */
#include "tsf_com.inc"
#endif

#ifdef CUIME_TSF_STUB
/* ───────── 스텁 검증 main ───────── */
#include <iostream>
using namespace cuime;
using namespace cuime::tsf;
static int fails = 0;
static void chk(const std::string& n, const std::string& g, const std::string& e) {
    if (g != e) { std::cout << "❌ " << n << " got=" << g << " exp=" << e << "\n"; fails++; }
    else std::cout << "✅ " << n << "\n";
}
static void chki(const std::string& n, long g, long e) { chk(n, std::to_string(g), std::to_string(e)); }

int main() {
    // 1) 키 매핑 — 상단 숫자열
    chk("상단 숫자 1", mapVirtualKey(0x31, PadMode::Phone), "K1");
    chk("상단 숫자 0", mapVirtualKey(0x30, PadMode::Phone), "K0");
    // 2) 숫자패드 전화기 논리 매핑 (docs/03 §2.2)
    chk("NumPad7 -> K1 (전화기)", mapVirtualKey(VK_NUMPAD0 + 7, PadMode::Phone), "K1");
    chk("NumPad8 -> K2", mapVirtualKey(VK_NUMPAD0 + 8, PadMode::Phone), "K2");
    chk("NumPad1 -> K7", mapVirtualKey(VK_NUMPAD0 + 1, PadMode::Phone), "K7");
    chk("NumPad5 -> K5 (중앙 불변)", mapVirtualKey(VK_NUMPAD0 + 5, PadMode::Phone), "K5");
    chk("NumPad0 -> K0", mapVirtualKey(VK_NUMPAD0, PadMode::Phone), "K0");
    // 3) NumPad 물리 매핑 모드
    chk("NumPad7 -> K7 (물리)", mapVirtualKey(VK_NUMPAD0 + 7, PadMode::NumPad), "K7");
    // 4) 기능키
    chk("VK_BACK", mapVirtualKey(VK_BACK, PadMode::Phone), "BACK");
    chk("VK_RETURN", mapVirtualKey(VK_RETURN, PadMode::Phone), "ENTER");
    chk("VK_LEFT", mapVirtualKey(VK_LEFT, PadMode::Phone), "LEFT");
    chk("VK_HANJA", mapVirtualKey(VK_HANJA, PadMode::Phone), "HANJA");
    chk("미매핑 키", mapVirtualKey(0x41, PadMode::Phone), "");

    // 5) 조합 — 물리 숫자열로 '나무위키'
    { CompositionHost h;
      for (UINT vk : {0x35,0x31,0x32,0x30,0x30,0x33,0x32,0x30,0x33,0x32,0x31,0x34,0x34,0x31})
          h.onKeyDown(vk, false);
      h.shell.core = press(h.shell.core, "COMMIT"); h.sync();
      chk("TSF 조합 나무위키", h.pendingCommit + h.lastPreedit, u8"나무위키"); }

    // 6) 숫자패드(전화기 매핑)로 동일 입력
    { CompositionHost h;
      // 나무위키 = 5 1 2 0 0 3 2 0 3 2 1 4 4 1 -> NumPad 로 환산
      auto np = [](int phone){ switch(phone){case 1:return 7;case 2:return 8;case 3:return 9;
                                             case 7:return 1;case 8:return 2;case 9:return 3;
                                             default:return phone;} };
      for (int p : {5,1,2,0,0,3,2,0,3,2,1,4,4,1})
          h.onKeyDown(UINT(VK_NUMPAD0 + np(p)), false);
      h.shell.core = press(h.shell.core, "COMMIT"); h.sync();
      chk("숫자패드 조합 동일", h.pendingCommit + h.lastPreedit, u8"나무위키"); }

    // 7) preedit / commit 분리
    { CompositionHost h;
      h.onKeyDown(0x34, false);                    // ㄱ
      chki("조합 시작", h.needsComposition() ? 1 : 0, 1);
      chk("preedit", h.lastPreedit, u8"ㄱ");
      h.onKeyDown(0x31, false); h.onKeyDown(0x32, false);   // ㅏ
      chk("preedit 가", h.lastPreedit, u8"가");
      h.onKeyDown(VK_SPACE, false);
      chk("공백 -> 커밋", h.takeCommit(), u8"가 ");
      chki("조합 종료", h.needsComposition() ? 1 : 0, 0); }

    // 8) 한자 변환 (실제 사전)
    { CompositionHost h;
      h.selectionForTest = u8"대한민국";
      h.onKeyDown(VK_HANJA, false);
      h.onKeyUp(VK_HANJA);
      chki("한자 후보 수", (long)h.shell.candidates.size(), 1);
      chk("한자 후보", h.shell.candidates.empty() ? "" : h.shell.candidates[0], u8"大韓民國"); }
    { CompositionHost h;
      h.selectionForTest = u8"국";
      h.onKeyDown(VK_HANJA, false); h.onKeyUp(VK_HANJA);
      chki("글자 후보 수 (실제 사전)", (long)h.shell.candidates.size(), 6);
      chk("첫 후보", h.shell.candidates[0], u8"國"); }

    // 9) 漢 + 방향키 코드 -> 레이아웃 전환
    { CompositionHost h;
      h.onKeyDown(VK_HANJA, false);
      h.onKeyDown(VK_RIGHT, false);
      h.onKeyUp(VK_HANJA);
      chki("코드 -> 영어", (long)h.shell.layout, 1);
      chki("코드 후 팔레트 미발생", h.shell.paletteOpen ? 1 : 0, 0); }

    // 10) M1 전송 가드
    { CompositionHost h; h.shell.cfg.enterIsSend = true; h.shell.nowMs = 1000;
      h.onKeyDown(0x34, false); h.onKeyDown(0x31, false); h.onKeyDown(0x32, false);
      h.onKeyDown(VK_BACK, false);
      h.shell.nowMs = 1100;
      bool eaten = h.onKeyDown(VK_RETURN, false);
      chki("M1 엔터 소비(차단)", eaten ? 1 : 0, 1);
      chki("M1 경고", h.shell.warning.empty() ? 0 : 1, 1); }

    // 11) UTF-16 변환
    { auto w = toUtf16(u8"가A");
      chki("UTF-16 길이", (long)w.size(), 2);
      chki("UTF-16 가", (long)w[0], 0xAC00);
      chki("UTF-16 A", (long)w[1], 0x41); }

    std::cout << "\n" << (fails == 0
        ? "✅ TSF 텍스트 서비스 전 항목 통과 (키매핑·조합·한자·코드·M1)"
        : "❌ 실패") << "\n";
    return fails ? 1 : 0;
}
#endif
