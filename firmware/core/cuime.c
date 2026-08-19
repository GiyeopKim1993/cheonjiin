/* 천지인 펌웨어 코어 구현 — 순수 C99, 하드웨어 비의존 */
#include "cuime.h"
#include "cuime_tables.h"

/* ── 내부 유틸 ── */

static void ev_push(cuime_evlist_t *o, cuime_evtype_t t, uint32_t cp,
                    cuime_key_t k, cuime_layout_t ly)
{
    if (!o || o->count >= CUIME_MAX_EVENTS) return;
    cuime_event_t *e = &o->ev[o->count++];
    e->type = t; e->codepoint = cp; e->key = k; e->layout = ly;
}

static void ev_clear(cuime_evlist_t *o) { if (o) o->count = 0; }

/* 자음 키(K4..K9,K0) -> 순환표 인덱스 0..6. 자음 키가 아니면 -1 */
static int ckey_index(cuime_key_t k)
{
    switch (k) {
        case CUIME_KEY_4: return 0;
        case CUIME_KEY_5: return 1;
        case CUIME_KEY_6: return 2;
        case CUIME_KEY_7: return 3;
        case CUIME_KEY_8: return 4;
        case CUIME_KEY_9: return 5;
        case CUIME_KEY_0: return 6;
        default: return -1;
    }
}

/* 모음 키 -> 전이표 열 0..2. 모음 키가 아니면 -1 */
static int vkey_index(cuime_key_t k)
{
    switch (k) {
        case CUIME_KEY_1: return 0;   /* ㅣ */
        case CUIME_KEY_2: return 1;   /* ㆍ */
        case CUIME_KEY_3: return 2;   /* ㅡ */
        default: return -1;
    }
}

static int find_idx(const uint16_t *arr, int n, uint32_t v)
{
    for (int i = 0; i < n; i++) if (arr[i] == v) return i;
    return -1;
}

/* 조합 완성: cho(1-base) + vstate -> 유니코드 음절. 미완성이면 자모 단독 */
static uint32_t compose_now(const cuime_t *st)
{
    uint8_t jung = (st->vstate < CUIME_VSTATE_COUNT)
                 ? CUIME_VSTATE_JUNG[st->vstate] : 0xFF;
    if (st->cho && jung != 0xFF)
        return 0xAC00u + ((uint32_t)(st->cho - 1) * 21u + jung) * 28u + st->jong;
    if (st->cho) return CUIME_CHO[st->cho - 1];
    if (jung != 0xFF) return CUIME_JUNG[jung];
    return 0;
}

uint32_t cuime_preedit(const cuime_t *st) { return compose_now(st); }

static void clear_compose(cuime_t *st)
{
    st->cho = 0; st->vstate = CUIME_VS_NONE; st->jong = 0; st->slot = 0;
}

/* 멀티탭 세션 종료 (스냅샷 무효화). 조합 내용은 건드리지 않는다. */
static void end_multitap(cuime_t *st)
{
    st->last_key = -1; st->tap = 0; st->has_snap = false;
}

/* 보류된 확정을 실제로 내보낸다 (멀티탭 세션 종료 시) */
static void release_pending(cuime_t *st, cuime_evlist_t *out)
{
    if (st->pending_commit) {
        ev_push(out, CUIME_EV_COMMIT, st->pending_commit,
                CUIME_KEY__COUNT, CUIME_LAYOUT_HANGUL);
        st->pending_commit = 0;
    }
}

/* 현재 조합을 확정 이벤트로 내보내고 비운다 */
static void flush_internal(cuime_t *st, cuime_evlist_t *out)
{
    uint32_t cp = compose_now(st);
    if (cp) ev_push(out, CUIME_EV_COMMIT, cp, CUIME_KEY__COUNT, CUIME_LAYOUT_HANGUL);
    clear_compose(st);
}

/* 외부 확정: 조합 + 멀티탭 세션 모두 종료 */
static void flush_all(cuime_t *st, cuime_evlist_t *out)
{
    release_pending(st, out);
    flush_internal(st, out);
    end_multitap(st);
}

void cuime_flush(cuime_t *st, cuime_evlist_t *out)
{
    ev_clear(out);
    flush_all(st, out);
}

static void emit_preedit(cuime_t *st, cuime_evlist_t *out)
{
    ev_push(out, CUIME_EV_PREEDIT, compose_now(st),
            CUIME_KEY__COUNT, CUIME_LAYOUT_HANGUL);
}

static void take_snapshot(cuime_t *st)
{
    st->snap_cho = st->cho; st->snap_vstate = st->vstate;
    st->snap_jong = st->jong; st->snap_slot = st->slot;
    st->snap_pending = st->pending_commit;
    st->has_snap = true;
}

static void restore_snapshot(cuime_t *st)
{
    st->cho = st->snap_cho; st->vstate = st->snap_vstate;
    st->jong = st->snap_jong; st->slot = st->snap_slot;
}

/* 자음 자모를 현재 조합에 결합. 새 글자로 넘어가면 확정 이벤트 발생 */
static void attach_consonant(cuime_t *st, uint32_t jamo, cuime_evlist_t *out)
{
    /* 빈 초성 자리 */
    if (st->slot == 0 && st->cho == 0) {
        int i = find_idx(CUIME_CHO, CUIME_CHO_COUNT, jamo);
        if (i >= 0) { st->cho = (uint8_t)(i + 1); st->slot = 0; }
        return;
    }
    /* 종성이 이미 있음 -> 겹받침 시도 */
    if (st->slot == 2 && st->jong) {
        uint32_t cur = CUIME_JONG[st->jong];
        for (int i = 0; i < CUIME_CLUSTER_COUNT; i++) {
            if (CUIME_CLUSTER[i][1] == cur && CUIME_CLUSTER[i][2] == jamo) {
                int j = find_idx(CUIME_JONG, CUIME_JONG_COUNT, CUIME_CLUSTER[i][0]);
                if (j > 0) { st->jong = (uint8_t)j; st->slot = 2; return; }
            }
        }
        flush_internal(st, out);
        int i = find_idx(CUIME_CHO, CUIME_CHO_COUNT, jamo);
        if (i >= 0) { st->cho = (uint8_t)(i + 1); st->slot = 0; }
        return;
    }
    /* 초성+중성 완성 -> 종성 자리 */
    uint8_t jung = (st->vstate < CUIME_VSTATE_COUNT)
                 ? CUIME_VSTATE_JUNG[st->vstate] : 0xFF;
    if (st->cho && jung != 0xFF) {
        int j = find_idx(CUIME_JONG, CUIME_JONG_COUNT, jamo);
        if (j > 0) { st->jong = (uint8_t)j; st->slot = 2; return; }
        flush_internal(st, out);
        int i = find_idx(CUIME_CHO, CUIME_CHO_COUNT, jamo);
        if (i >= 0) { st->cho = (uint8_t)(i + 1); st->slot = 0; }
        return;
    }
    /* 그 외 -> 확정 후 새 글자 */
    flush_internal(st, out);
    {
        int i = find_idx(CUIME_CHO, CUIME_CHO_COUNT, jamo);
        if (i >= 0) { st->cho = (uint8_t)(i + 1); st->slot = 0; }
    }
}

/* ── 공개 API ── */

void cuime_init(cuime_t *st)
{
    if (!st) return;
    for (size_t i = 0; i < sizeof(*st); i++) ((uint8_t *)st)[i] = 0;
    st->last_key = -1;
    st->en_key   = -1;
    st->layout   = CUIME_LAYOUT_HANGUL;
}

bool cuime_decompose(uint32_t s, uint32_t *cho, uint32_t *jung, uint32_t *jong)
{
    if (s < 0xAC00u || s > 0xD7A3u) return false;
    uint32_t c = s - 0xAC00u;
    if (cho)  *cho  = CUIME_CHO[c / 588u];
    if (jung) *jung = CUIME_JUNG[(c % 588u) / 28u];
    if (jong) *jong = CUIME_JONG[c % 28u];
    return true;
}

bool cuime_to_dubeolsik(uint32_t jamo, char *a, char *b)
{
    for (int i = 0; i < CUIME_DUBEOL_COUNT; i++) {
        if (CUIME_DUBEOL[i].jamo == jamo) {
            if (a) *a = CUIME_DUBEOL[i].a;
            if (b) *b = CUIME_DUBEOL[i].b;
            return true;
        }
    }
    return false;
}

/* 백스페이스: 자모 단위 역추적 (docs/00 §3.5) */
static void do_backspace(cuime_t *st, cuime_evlist_t *out)
{
    release_pending(st, out);
    end_multitap(st);

    if (st->jong) {
        /* 겹받침이면 앞 성분만 남긴다 */
        uint32_t cur = CUIME_JONG[st->jong];
        for (int i = 0; i < CUIME_CLUSTER_COUNT; i++) {
            if (CUIME_CLUSTER[i][0] == cur) {
                int j = find_idx(CUIME_JONG, CUIME_JONG_COUNT, CUIME_CLUSTER[i][1]);
                st->jong = (j > 0) ? (uint8_t)j : 0;
                emit_preedit(st, out);
                return;
            }
        }
        st->jong = 0; st->slot = 1;
        emit_preedit(st, out);
        return;
    }
    if (st->vstate != CUIME_VS_NONE) {
        uint8_t p = CUIME_VPREV[st->vstate];
        st->vstate = (p == 0xFF) ? CUIME_VS_NONE : p;
        st->slot = (st->vstate == CUIME_VS_NONE) ? 0 : 1;
        emit_preedit(st, out);
        return;
    }
    if (st->cho) {
        st->cho = 0; st->slot = 0;
        emit_preedit(st, out);
        return;
    }
    /* 조합이 없으면 호스트에 백스페이스 전달 */
    ev_push(out, CUIME_EV_BACKSPACE, 0, CUIME_KEY_BACK, CUIME_LAYOUT_HANGUL);
}

/* M1 전송 가드 (docs/00 §7): ⌫ 직후 300ms 내 ↵ 1회 무시 */
#define CUIME_SEND_GUARD_MS 300

void cuime_key_down(cuime_t *st, cuime_key_t key, cuime_evlist_t *out)
{
    if (!st) return;
    ev_clear(out);

    /* 漢 키: 누를 때는 상태만 기록 (뗄 때 판정 — docs/00 §5) */
    if (key == CUIME_KEY_HANJA) {
        st->hj_down = true; st->hj_consumed = false;
        return;
    }

    /* 방향키: 코드(漢+방향) 이면 레이아웃 전환 */
    if (key == CUIME_KEY_LEFT || key == CUIME_KEY_RIGHT) {
        if (st->hj_down) {
            if (!st->hj_consumed) {
                int d = (key == CUIME_KEY_RIGHT) ? 1 : (CUIME_LAYOUT__COUNT - 1);
                st->layout = (uint8_t)((st->layout + d) % CUIME_LAYOUT__COUNT);
                st->hj_consumed = true;
                flush_all(st, out);
                st->en_key = -1; st->en_tap = 0;
                ev_push(out, CUIME_EV_LAYOUT, 0, key, (cuime_layout_t)st->layout);
            }
            return;
        }
        /* 조합 중이면 확정, 아니면 커서 이동 */
        if (compose_now(st)) { flush_all(st, out); return; }
        end_multitap(st);
        ev_push(out, CUIME_EV_CONTROL, 0, key, (cuime_layout_t)st->layout);
        return;
    }

    if (key == CUIME_KEY_TIMEOUT) {
        release_pending(st, out);
        end_multitap(st);
        st->en_key = -1; st->en_tap = 0;
        return;
    }
    if (key == CUIME_KEY_COMMIT) { flush_all(st, out); return; }

    if (key == CUIME_KEY_BACK) {
        st->last_back_ms = st->now_ms ? st->now_ms : 1;
        st->en_key = -1; st->en_tap = 0;
        do_backspace(st, out);
        return;
    }

    if (key == CUIME_KEY_ENTER) {
        /* M1 가드: ⌫ 직후면 1회 무시 */
        if (st->last_back_ms &&
            (uint32_t)(st->now_ms - st->last_back_ms) < CUIME_SEND_GUARD_MS) {
            st->last_back_ms = 0;
            return;                      /* 이벤트 없음 = 차단 */
        }
        flush_all(st, out);
        st->en_key = -1; st->en_tap = 0;
        ev_push(out, CUIME_EV_CONTROL, 0, key, (cuime_layout_t)st->layout);
        return;
    }

    if (key == CUIME_KEY_SPACE) {
        flush_all(st, out);
        st->en_key = -1; st->en_tap = 0;
        ev_push(out, CUIME_EV_CONTROL, 0, key, (cuime_layout_t)st->layout);
        return;
    }

    /* ── 글자 키 ── */
    if (st->layout != CUIME_LAYOUT_HANGUL) {
        /* 영문/숫자는 HAL 이 처리 (코어는 한글 조합 담당) */
        ev_push(out, CUIME_EV_CONTROL, 0, key, (cuime_layout_t)st->layout);
        return;
    }

    int vi = vkey_index(key);
    if (vi >= 0) {
        /* 받침 뒤 모음 -> 연음(도깨비불) */
        if (st->jong) {
            uint32_t cur = CUIME_JONG[st->jong];
            uint32_t moved = cur, rest = 0;
            for (int i = 0; i < CUIME_CLUSTER_COUNT; i++) {
                if (CUIME_CLUSTER[i][0] == cur) {
                    rest = CUIME_CLUSTER[i][1];
                    moved = CUIME_CLUSTER[i][2];
                    break;
                }
            }
            int rj = rest ? find_idx(CUIME_JONG, CUIME_JONG_COUNT, rest) : 0;
            st->jong = (rj > 0) ? (uint8_t)rj : 0;
            release_pending(st, out);
            flush_internal(st, out);
            end_multitap(st);
            int ci = find_idx(CUIME_CHO, CUIME_CHO_COUNT, moved);
            if (ci >= 0) { st->cho = (uint8_t)(ci + 1); st->slot = 1; }
        }
        release_pending(st, out);
        uint8_t nxt = CUIME_VTRANS[st->vstate][vi];
        if (nxt == 0xFF) {
            flush_internal(st, out);
            nxt = CUIME_VTRANS[CUIME_VS_NONE][vi];
        }
        st->vstate = nxt; st->slot = 1;
        end_multitap(st);
        emit_preedit(st, out);
        return;
    }

    int ci = ckey_index(key);
    if (ci >= 0) {
        uint8_t tap;
        if (st->last_key == ci && st->has_snap) {
            /* 같은 키 연타 = 되감기. 보류 확정도 함께 취소한다.
             * (참조 구현은 committed 문자열을 되돌린다 — docs/00 §8.1)
             * 호스트로 아직 보내지 않았으므로 취소가 가능하다. */
            tap = (uint8_t)((st->tap + 1) % CUIME_CYCLE_LEN[ci]);
            restore_snapshot(st);
            st->pending_commit = st->snap_pending;
        } else {
            /* 다른 키 = 이전 세션 종료 -> 보류분 확정 */
            release_pending(st, out);
            tap = 0;
        }
        take_snapshot(st);

        /* attach 가 만들어내는 확정은 보류 버퍼로 돌린다 */
        cuime_evlist_t tmp; tmp.count = 0;
        attach_consonant(st, CUIME_CYCLE[ci][tap], &tmp);
        for (uint8_t i = 0; i < tmp.count; i++) {
            if (tmp.ev[i].type == CUIME_EV_COMMIT) {
                /* 직전 보류분이 있으면 그건 확정된 것 */
                release_pending(st, out);
                st->pending_commit = tmp.ev[i].codepoint;
            } else {
                ev_push(out, tmp.ev[i].type, tmp.ev[i].codepoint,
                        tmp.ev[i].key, tmp.ev[i].layout);
            }
        }
        st->last_key = (int8_t)ci; st->tap = tap; st->has_snap = true;
        emit_preedit(st, out);
        return;
    }
}

void cuime_key_up(cuime_t *st, cuime_key_t key, cuime_evlist_t *out)
{
    if (!st) return;
    ev_clear(out);
    if (key != CUIME_KEY_HANJA) return;

    bool consumed = st->hj_consumed;
    st->hj_down = false; st->hj_consumed = false;
    if (consumed) return;               /* 코드로 소비됨 -> 팔레트/한자 없음 */

    /* 멀티탭 세션 종료 후 한자 변환 요청 */
    release_pending(st, out);
    end_multitap(st);
    ev_push(out, CUIME_EV_HANJA_REQ, compose_now(st),
            CUIME_KEY_HANJA, (cuime_layout_t)st->layout);
}

void cuime_key_longpress(cuime_t *st, cuime_key_t key, cuime_evlist_t *out)
{
    if (!st) return;
    ev_clear(out);
    if (st->layout != CUIME_LAYOUT_HANGUL) {
        ev_push(out, CUIME_EV_CONTROL, 0, key, (cuime_layout_t)st->layout);
        return;
    }
    int ci = ckey_index(key);
    if (ci < 0) return;
    /* key_down 에서 이미 1탭이 들어갔다. 같은 키면 그 입력을 되감은 뒤
     * 순환열 마지막 자모를 결합한다 (docs/00 §3.2). */
    if (st->last_key == ci && st->has_snap) {
        restore_snapshot(st);
        st->pending_commit = st->snap_pending;
    }
    release_pending(st, out);
    attach_consonant(st, CUIME_LONGPRESS[ci], out);
    end_multitap(st);
    emit_preedit(st, out);
}
