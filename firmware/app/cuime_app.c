/* 천지인 펌웨어 앱 계층 — 정책·타이머·출력 변환
 *
 * 책임 (docs/00-확정스펙.md §8.1):
 *   - 멀티탭/롱프레스 타이머 소유 → 코어에 CUIME_KEY_TIMEOUT 주입
 *   - 코어 이벤트를 HAL 출력으로 변환 (두벌식 HID / 유니코드)
 *   - 영문·숫자 레이아웃 멀티탭 (코어는 한글만 담당)
 *   - 한자 후보 관리
 *
 * 하드웨어 의존성 없음. HAL 인터페이스만 사용한다.
 */
#include "cuime_app.h"
#include "../core/cuime_tables.h"
#include <string.h>

/* E.161 영문 멀티탭 (docs/00 §3.2) */
static const char *EN_CYCLE[10] = {
    ".,?!'",   /* K1 */
    "abc",     /* K2 */
    "def",     /* K3 */
    "ghi",     /* K4 */
    "jkl",     /* K5 */
    "mno",     /* K6 */
    "pqrs",    /* K7 */
    "tuv",     /* K8 */
    "wxyz",    /* K9 */
    ""         /* K0 = shift */
};

/* 기호 팔레트 3페이지 × 16 (docs/00 §5) */
static const uint32_t SYMBOLS[3][16] = {
    { '.', ',', '?', '!', 0x223C, 0x2026, '\'', '"',
      '(', ')', ':', ';', '-', '/', '@', '#' },
    { '+', '-', 0x00D7, 0x00F7, '=', 0x00B1, '%', 0x00B0,
      0x2103, 0x20A9, '$', 0x20AC, 0x00A5, 0x2116, 0x203B, 0x2605 },
    { '[', ']', '{', '}', 0x3008, 0x3009, 0x300A, 0x300B,
      0x300C, 0x300D, 0x2190, 0x2192, 0x2191, 0x2193, 0x2194, 0x21D2 }
};

/* 키 인덱스 -> 숫자 문자 */
static char key_digit(cuime_key_t k)
{
    if (k == CUIME_KEY_0) return '0';
    if (k >= CUIME_KEY_1 && k <= CUIME_KEY_9)
        return (char)('1' + (k - CUIME_KEY_1));
    return 0;
}

/* ── 출력 ── */

/* 자모 1개를 호스트로 전송. 모드에 따라 두벌식 HID 또는 유니코드 */
static void emit_jamo(uint32_t jamo)
{
    if (hal_out_mode() == HAL_OUT_UNICODE_TEXT) {
        hal_out_unicode(jamo);
        return;
    }
    char a = 0, b = 0;
    if (cuime_to_dubeolsik(jamo, &a, &b)) {
        /* 대문자 = Shift 필요 (쌍자음 ㅃㅉㄸㄲㅆ, ㅒㅖ) */
        if (a) hal_out_ascii((char)(a >= 'A' && a <= 'Z' ? a + 32 : a),
                             (bool)(a >= 'A' && a <= 'Z'));
        if (b) hal_out_ascii((char)(b >= 'A' && b <= 'Z' ? b + 32 : b),
                             (bool)(b >= 'A' && b <= 'Z'));
    }
}

/* 완성 음절을 전송. 두벌식 모드면 자모로 분해해 보낸다. */
static void emit_syllable(uint32_t cp)
{
    if (hal_out_mode() == HAL_OUT_UNICODE_TEXT) {
        hal_out_unicode(cp);
        return;
    }
    uint32_t cho, jung, jong;
    if (cuime_decompose(cp, &cho, &jung, &jong)) {
        emit_jamo(cho);
        emit_jamo(jung);
        if (jong) emit_jamo(jong);
    } else {
        emit_jamo(cp);      /* 단독 자모 */
    }
}

/* ── 앱 ── */

void cuime_app_init(cuime_app_t *app)
{
    memset(app, 0, sizeof(*app));
    cuime_init(&app->core);
    app->cfg.multitap_ms  = 800;
    app->cfg.longpress_ms = 300;
    app->cfg.repeat_ms    = 60;
    app->en_key = -1;
    app->held_key = -1;
}

/* 코어 이벤트를 HAL 출력으로 */
static void drain(cuime_app_t *app, const cuime_evlist_t *ev)
{
    for (uint8_t i = 0; i < ev->count; i++) {
        const cuime_event_t *e = &ev->ev[i];
        switch (e->type) {
        case CUIME_EV_COMMIT:
            if (e->codepoint) emit_syllable(e->codepoint);
            break;
        case CUIME_EV_PREEDIT:
            hal_ui_preedit(e->codepoint);
            break;
        case CUIME_EV_BACKSPACE:
            hal_out_special(HAL_KEYCODE_BACKSPACE);
            break;
        case CUIME_EV_CONTROL:
            if (e->key == CUIME_KEY_ENTER)      hal_out_special(HAL_KEYCODE_ENTER);
            else if (e->key == CUIME_KEY_SPACE) hal_out_special(HAL_KEYCODE_SPACE);
            else if (e->key == CUIME_KEY_LEFT)  hal_out_special(HAL_KEYCODE_LEFT);
            else if (e->key == CUIME_KEY_RIGHT) hal_out_special(HAL_KEYCODE_RIGHT);
            break;
        case CUIME_EV_LAYOUT:
            app->en_key = -1; app->en_tap = 0;
            hal_ui_layout(e->layout);
            break;
        case CUIME_EV_HANJA_REQ:
            cuime_app_hanja(app, e->codepoint);
            break;
        default: break;
        }
    }
}

/* 영문 레이아웃 멀티탭 (코어 밖에서 처리) */
static void english_tap(cuime_app_t *app, cuime_key_t key)
{
    int idx = (key == CUIME_KEY_0) ? 9 : (int)(key - CUIME_KEY_1);
    if (idx < 0 || idx > 9) return;
    if (idx == 9) { app->shift = !app->shift; return; }   /* K0 = shift */

    const char *cyc = EN_CYCLE[idx];
    size_t len = strlen(cyc);
    if (len == 0) return;

    if (app->en_key == idx) {
        hal_out_special(HAL_KEYCODE_BACKSPACE);           /* 직전 글자 취소 */
        app->en_tap = (uint8_t)((app->en_tap + 1) % len);
    } else {
        app->en_key = (int8_t)idx;
        app->en_tap = 0;
    }
    char c = cyc[app->en_tap];
    hal_out_ascii(c, app->shift);
    app->shift = false;
}

static void english_long(cuime_app_t *app, cuime_key_t key)
{
    int idx = (key == CUIME_KEY_0) ? 9 : (int)(key - CUIME_KEY_1);
    if (idx < 0 || idx >= 9) return;
    const char *cyc = EN_CYCLE[idx];
    size_t len = strlen(cyc);
    if (!len) return;
    app->en_key = -1; app->en_tap = 0;
    hal_out_ascii(cyc[len - 1], app->shift);              /* 순환열 마지막 */
    app->shift = false;
}

void cuime_app_key_down(cuime_app_t *app, cuime_key_t key)
{
    cuime_evlist_t ev; ev.count = 0;
    uint32_t now = hal_millis();
    cuime_set_time(&app->core, now);

    /* 후보창이 열려 있으면 방향키/엔터를 가로챈다 (docs/00 §4) */
    if (app->cand_count) {
        if (key == CUIME_KEY_LEFT || key == CUIME_KEY_RIGHT) {
            int d = (key == CUIME_KEY_RIGHT) ? 1 : -1;
            app->cand_index = (uint8_t)((app->cand_index + d + app->cand_count)
                                        % app->cand_count);
            hal_ui_candidates(app->cand, app->cand_count, app->cand_index);
            return;
        }
        if (key == CUIME_KEY_ENTER || key == CUIME_KEY_SPACE) {
            emit_syllable(app->cand[app->cand_index]);
            app->cand_count = 0;
            hal_ui_candidates(NULL, 0, 0);
            return;
        }
        if (key == CUIME_KEY_BACK) {
            app->cand_count = 0;
            hal_ui_candidates(NULL, 0, 0);
            return;
        }
        app->cand_count = 0;      /* 그 외 키는 후보 취소 후 계속 */
        hal_ui_candidates(NULL, 0, 0);
    }

    /* 팔레트가 열려 있으면 글자키로 기호 선택 */
    if (app->palette_open) {
        if (key == CUIME_KEY_LEFT || key == CUIME_KEY_RIGHT) {
            int d = (key == CUIME_KEY_RIGHT) ? 1 : 2;
            app->palette_page = (uint8_t)((app->palette_page + d) % 3);
            return;
        }
        char dg = key_digit(key);
        if (dg) {
            int i = (dg == '0') ? 9 : (dg - '1');
            emit_syllable(SYMBOLS[app->palette_page][i]);
            return;
        }
        if (key == CUIME_KEY_BACK) { app->palette_open = false; return; }
    }

    /* 영문/숫자 레이아웃은 앱이 처리 */
    if (app->core.layout != CUIME_LAYOUT_HANGUL &&
        key <= CUIME_KEY_0 && !app->core.hj_down) {
        if (app->core.layout == CUIME_LAYOUT_ENGLISH) english_tap(app, key);
        else { char d = key_digit(key); if (d) hal_out_ascii(d, false); }
        app->multitap_deadline = now + app->cfg.multitap_ms;
        app->held_key = (int8_t)key;
        app->hold_start = now;
        return;
    }

    cuime_key_down(&app->core, key, &ev);
    drain(app, &ev);

    /* 멀티탭 타이머 재장전 (코어는 시간을 모른다) */
    if (key <= CUIME_KEY_0) app->multitap_deadline = now + app->cfg.multitap_ms;
    app->held_key = (int8_t)key;
    app->hold_start = now;
    app->long_fired = false;
}

void cuime_app_key_up(cuime_app_t *app, cuime_key_t key)
{
    cuime_evlist_t ev; ev.count = 0;
    cuime_set_time(&app->core, hal_millis());
    app->held_key = -1;

    if (app->long_fired) { app->long_fired = false; return; }

    if (key == CUIME_KEY_HANJA) {
        cuime_key_up(&app->core, key, &ev);
        drain(app, &ev);
    }
}

/* 주기 호출: 타이머 만료 처리 (메인 루프에서) */
void cuime_app_tick(cuime_app_t *app)
{
    uint32_t now = hal_millis();
    cuime_set_time(&app->core, now);

    /* 롱프레스 */
    if (app->held_key >= 0 && !app->long_fired &&
        (uint32_t)(now - app->hold_start) >= app->cfg.longpress_ms) {
        app->long_fired = true;
        cuime_key_t k = (cuime_key_t)app->held_key;
        if (k == CUIME_KEY_HANJA) {
            app->core.hj_consumed = true;
            app->palette_open = true;
            app->palette_page = 0;
        } else if (app->core.layout == CUIME_LAYOUT_ENGLISH && k <= CUIME_KEY_0) {
            /* key_down 에서 이미 1탭이 출력됐다 -> 취소 후 마지막 글자 */
            if (app->en_key >= 0) {
                hal_out_special(HAL_KEYCODE_BACKSPACE);
                app->en_key = -1; app->en_tap = 0;
            }
            english_long(app, k);
        } else {
            cuime_evlist_t ev; ev.count = 0;
            cuime_key_longpress(&app->core, k, &ev);
            drain(app, &ev);
        }
        app->multitap_deadline = 0;
        return;
    }

    /* 멀티탭 타임아웃 -> 코어에 주입 */
    if (app->multitap_deadline && (int32_t)(now - app->multitap_deadline) >= 0) {
        app->multitap_deadline = 0;
        cuime_evlist_t ev; ev.count = 0;
        cuime_key_down(&app->core, CUIME_KEY_TIMEOUT, &ev);
        drain(app, &ev);
        app->en_key = -1; app->en_tap = 0;
    }
}

/* 한자 변환 요청 처리. 사전은 앱이 주입받는다(펌웨어 용량 정책) */
void cuime_app_hanja(cuime_app_t *app, uint32_t syllable)
{
    if (!app->hanja_lookup || !syllable) {
        app->palette_open = true;   /* 사전 없으면 기호 팔레트 */
        app->palette_page = 0;
        return;
    }
    app->cand_count = app->hanja_lookup(syllable, app->cand, CUIME_APP_MAX_CAND);
    app->cand_index = 0;
    if (app->cand_count) hal_ui_candidates(app->cand, app->cand_count, 0);
    else { app->palette_open = true; app->palette_page = 0; }
}

/* 키 매트릭스 폴링 — 에지 검출 후 key_down/up 호출 */
void cuime_app_poll(cuime_app_t *app)
{
    hal_keymask_t now_mask = hal_keys_scan();
    hal_keymask_t changed = now_mask ^ app->prev_mask;

    for (int i = 0; i < CUIME_KEY__COUNT; i++) {
        hal_keymask_t bit = (hal_keymask_t)1u << i;
        if (!(changed & bit)) continue;
        if (now_mask & bit) cuime_app_key_down(app, (cuime_key_t)i);
        else                cuime_app_key_up(app, (cuime_key_t)i);
    }
    app->prev_mask = now_mask;
    cuime_app_tick(app);
}
