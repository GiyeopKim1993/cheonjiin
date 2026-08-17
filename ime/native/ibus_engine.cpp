// 천지인 Linux IBus 엔진 — docs/00-확정스펙.md §9
//
// 셸 상태 머신은 shell.hpp 를 공유한다 (TSF·IMK 와 동일 코드).
// 이 파일은 IBus 프레임워크 바인딩만 담당한다.
//
// 빌드:
//   g++ -std=c++17 ibus_engine.cpp -o cuime-ibus $(pkg-config --cflags --libs ibus-1.0)

#include "shell.hpp"
#include "hanja.hpp"

#ifdef CUIME_IBUS_MAIN
/* ───────── 실제 IBus 바인딩 (ibus-1.0 헤더 필요) ─────────
 *
 * static void engine_process_key(IBusEngine* e, guint keyval, guint keycode, guint mods) {
 *     cuime::Shell* sh = shell_of(e);
 *     bool release = (mods & IBUS_RELEASE_MASK);
 *     const char* k = map_keyval(keyval);            // 1~0, Left, Right, BackSpace, Hangul_Hanja
 *     if (!k) return;
 *     if (!strcmp(k, "HANJA")) {
 *         if (release) {
 *             std::string sel = get_selection(e);
 *             sh->hanjaUp(sel, is_hangul(sel), cuime::hanja::lookup(sel));
 *         } else sh->hanjaDown();
 *     } else if (release) return;
 *     else if (!strcmp(k, "LEFT"))  sh->arrow(-1);
 *     else if (!strcmp(k, "RIGHT")) sh->arrow(1);
 *     else if (!strcmp(k, "BACK"))  sh->backspace();
 *     else if (!strcmp(k, "SPACE")) sh->space();
 *     else if (!strcmp(k, "ENTER")) sh->enter();
 *     else sh->tap(k);
 *
 *     // preedit 표시 + 확정
 *     ibus_engine_update_preedit_text(e,
 *         ibus_text_new_from_string(sh->preedit().c_str()),
 *         sh->preedit().length(), !sh->preedit().empty());
 *     if (!sh->core.committed.empty()) {
 *         ibus_engine_commit_text(e, ibus_text_new_from_string(sh->core.committed.c_str()));
 *         sh->core.committed.clear();
 *     }
 * }
 *
 * 멀티탭 타이머: g_timeout_add(sh->cfg.multitapMs, on_timeout, e) 에서 sh->timeout()
 * 롱프레스: 키 반복(IBUS_..._MASK) 감지 후 sh->longPress(k)
 */
int main() { return 0; }
#endif
