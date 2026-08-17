/* 천지인 펌웨어 앱 계층 — 하드웨어 무관 정책/타이머
 *
 * 보드 포팅 시 이 파일은 수정하지 않는다. HAL 만 구현하면 된다.
 */
#ifndef CUIME_APP_H
#define CUIME_APP_H

#include "../core/cuime.h"
#include "../hal/cuime_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CUIME_APP_MAX_CAND 16

/** 한자 사전 조회 콜백.
 *  펌웨어 용량 정책상 사전은 선택 사항이므로 앱이 주입받는다.
 *  @return 채운 후보 수 (0 = 없음)
 */
typedef uint8_t (*cuime_hanja_fn)(uint32_t syllable, uint32_t *out, uint8_t max);

typedef struct {
    uint16_t multitap_ms;     /* 기본 800 */
    uint16_t longpress_ms;    /* 기본 300 (docs/00 §10) */
    uint16_t repeat_ms;       /* 백스페이스 반복 */
} cuime_cfg_t;

typedef struct {
    cuime_t      core;
    cuime_cfg_t  cfg;

    /* 타이머 (앱이 소유 — 코어는 시간을 모른다) */
    uint32_t     multitap_deadline;
    uint32_t     hold_start;
    int8_t       held_key;
    bool         long_fired;

    /* 영문 멀티탭 */
    int8_t       en_key;
    uint8_t      en_tap;
    bool         shift;

    /* 한자 후보 */
    uint32_t     cand[CUIME_APP_MAX_CAND];
    uint8_t      cand_count;
    uint8_t      cand_index;
    cuime_hanja_fn hanja_lookup;

    /* 기호 팔레트 */
    bool         palette_open;
    uint8_t      palette_page;

    /* 키 스캔 에지 검출 */
    hal_keymask_t prev_mask;
} cuime_app_t;

void cuime_app_init(cuime_app_t *app);

/** 메인 루프에서 주기 호출 (권장 5~10ms). 스캔+에지검출+타이머 처리 */
void cuime_app_poll(cuime_app_t *app);

/* 직접 주입용 (테스트/시뮬레이터) */
void cuime_app_key_down(cuime_app_t *app, cuime_key_t key);
void cuime_app_key_up(cuime_app_t *app, cuime_key_t key);
void cuime_app_tick(cuime_app_t *app);
void cuime_app_hanja(cuime_app_t *app, uint32_t syllable);

#ifdef __cplusplus
}
#endif
#endif /* CUIME_APP_H */
