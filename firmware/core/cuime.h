/* 천지인 펌웨어 코어 — 하드웨어 비의존 조합 엔진
 *
 * 설계 계약 (docs/00-확정스펙.md §8.1):
 *   - 순수 상태 머신. malloc / float / 전역 가변 상태 / OS 호출 없음.
 *   - 코어는 시간을 모른다. 타이머는 HAL 계층이 소유하고 CUIME_KEY_TIMEOUT 으로 주입한다.
 *   - 참조 구현(Python/JS/Java/C++)과 동일한 테스트 벡터를 통과해야 한다.
 *
 * 이식성:
 *   C99 만 요구한다. ARM Cortex-M(0/3/4/33), ESP32(Xtensa/RISC-V), AVR, 호스트 어디서나 빌드된다.
 *   RAM 사용량: cuime_t 인스턴스 1개 = 약 40바이트. 테이블은 전부 const(플래시).
 */
#ifndef CUIME_H
#define CUIME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 논리 키 (물리 배선과 무관) ────────────────────────────
 * 4×4 확정 배치 (docs/00 §2):
 *    ㅣ    ㆍ    ㅡ    ⌫
 *   ㄱㅋ  ㄴㄹ  ㄷㅌ   ↵
 *   ㅂㅍ  ㅅㅎ  ㅈㅊ   漢
 *    ◀   ㅇㅁ   ▶    ␣
 */
typedef enum {
    CUIME_KEY_1 = 0,      /* ㅣ */
    CUIME_KEY_2,          /* ㆍ */
    CUIME_KEY_3,          /* ㅡ */
    CUIME_KEY_4,          /* ㄱㅋㄲ */
    CUIME_KEY_5,          /* ㄴㄹ   */
    CUIME_KEY_6,          /* ㄷㅌㄸ */
    CUIME_KEY_7,          /* ㅂㅍㅃ */
    CUIME_KEY_8,          /* ㅅㅎㅆ */
    CUIME_KEY_9,          /* ㅈㅊㅉ */
    CUIME_KEY_0,          /* ㅇㅁ   */
    CUIME_KEY_LEFT,       /* ◀ */
    CUIME_KEY_RIGHT,      /* ▶ 조합 확정 겸용 */
    CUIME_KEY_BACK,       /* ⌫ */
    CUIME_KEY_ENTER,      /* ↵ */
    CUIME_KEY_SPACE,      /* ␣ */
    CUIME_KEY_HANJA,      /* 漢 */
    CUIME_KEY__COUNT,

    /* 물리 키가 아닌 주입 이벤트 */
    CUIME_KEY_TIMEOUT = 100,   /* 멀티탭 타임아웃 (HAL 타이머가 주입) */
    CUIME_KEY_COMMIT  = 101    /* 강제 확정 */
} cuime_key_t;

/* 레이아웃 (docs/00 §5.2) */
typedef enum {
    CUIME_LAYOUT_HANGUL = 0,
    CUIME_LAYOUT_ENGLISH,
    CUIME_LAYOUT_NUMBER,
    CUIME_LAYOUT__COUNT
} cuime_layout_t;

/* 코어가 상위 계층에 알리는 출력 이벤트 */
typedef enum {
    CUIME_EV_NONE = 0,
    CUIME_EV_PREEDIT,     /* 조합 중 글자 변경 (codepoint 유효) */
    CUIME_EV_COMMIT,      /* 글자 확정      (codepoint 유효) */
    CUIME_EV_BACKSPACE,   /* 호스트에 백스페이스 1회 전송 */
    CUIME_EV_CONTROL,     /* Enter/Space/방향키 등 제어키 (key 유효) */
    CUIME_EV_LAYOUT,      /* 레이아웃 전환 (layout 유효) */
    CUIME_EV_HANJA_REQ    /* 한자 변환 요청 (상위에서 사전 조회) */
} cuime_evtype_t;

typedef struct {
    cuime_evtype_t type;
    uint32_t       codepoint;   /* PREEDIT / COMMIT */
    cuime_key_t    key;         /* CONTROL */
    cuime_layout_t layout;      /* LAYOUT */
} cuime_event_t;

/* 최대 이벤트 수: 한 키 입력이 만들 수 있는 이벤트 (확정+새 조합 등) */
#define CUIME_MAX_EVENTS 4

typedef struct {
    cuime_event_t ev[CUIME_MAX_EVENTS];
    uint8_t       count;
} cuime_evlist_t;

/* ── 조합 상태 (약 40바이트) ── */
typedef struct {
    uint8_t  cho;        /* 초성 인덱스+1, 0=없음 */
    uint8_t  vstate;     /* 모음 오토마타 상태 */
    uint8_t  jong;       /* 종성 인덱스, 0=없음 */
    uint8_t  slot;       /* 0=cho 1=jung 2=jong */

    int8_t   last_key;   /* 멀티탭 대상 자음 키 인덱스, -1=없음 */
    uint8_t  tap;        /* 순환 인덱스 */
    /* 멀티탭 되감기 스냅샷 (docs/00 §8.1) */
    uint8_t  snap_cho, snap_vstate, snap_jong, snap_slot;
    bool     has_snap;
    /* 멀티탭 중 발생한 확정은 즉시 보내지 않고 보류한다.
     * 연타로 되감기면 취소되고, 세션이 끝나야 실제로 나간다.
     * (HID 는 이미 보낸 키를 취소할 수 없으므로 필수) */
    uint32_t pending_commit;
    uint32_t snap_pending;

    uint8_t  layout;
    bool     shift;      /* 영문 대문자 */
    int8_t   en_key;     /* 영문 멀티탭 키, -1=없음 */
    uint8_t  en_tap;

    bool     hj_down;    /* 漢 키 눌림 (코드 판정) */
    bool     hj_consumed;

    uint32_t last_back_ms;   /* M1 전송 가드 (docs/00 §7) */
    uint32_t now_ms;         /* HAL 이 갱신 */
} cuime_t;

/* ── API ── */

/** 상태 초기화 */
void cuime_init(cuime_t *st);

/** 키 눌림. 발생한 이벤트를 out 에 채운다. */
void cuime_key_down(cuime_t *st, cuime_key_t key, cuime_evlist_t *out);

/** 키 뗌. 漢 키 판정에 필요(누를 때가 아니라 뗄 때 결정 — docs/00 §5). */
void cuime_key_up(cuime_t *st, cuime_key_t key, cuime_evlist_t *out);

/** 롱프레스 = 순환열 마지막 항목 (docs/00 §3.2) */
void cuime_key_longpress(cuime_t *st, cuime_key_t key, cuime_evlist_t *out);

/** 현재 조합 중 글자. 없으면 0 */
uint32_t cuime_preedit(const cuime_t *st);

/** 조합 강제 확정 */
void cuime_flush(cuime_t *st, cuime_evlist_t *out);

/** 현재 시각(ms) 주입 — M1 가드용. HAL 이 매 입력 전 호출 */
static inline void cuime_set_time(cuime_t *st, uint32_t ms) { st->now_ms = ms; }

/** 자모 -> 두벌식 ASCII (HID 전송용). b 가 0이 아니면 2타. 매핑 없으면 false */
bool cuime_to_dubeolsik(uint32_t jamo, char *a, char *b);

/** 유니코드 음절 -> 자모 3개 분해. 한글 음절이 아니면 false */
bool cuime_decompose(uint32_t syllable, uint32_t *cho, uint32_t *jung, uint32_t *jong);

#ifdef __cplusplus
}
#endif
#endif /* CUIME_H */
