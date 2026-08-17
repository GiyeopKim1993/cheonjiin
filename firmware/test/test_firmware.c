/* 펌웨어 통합 검증 — core + app + HAL(sim) 을 실제로 구동
 *
 * 실기기 없이 펌웨어 전체 경로를 검증한다:
 *   키 스캔 -> 에지 검출 -> 멀티탭/롱프레스 타이머 -> 조합 -> 두벌식 HID 출력
 */
#include "../app/cuime_app.h"
#include <stdio.h>
#include <string.h>

/* 시뮬레이터 훅 */
extern char sim_out[]; extern int sim_out_len;
extern uint32_t sim_preedit;
extern cuime_layout_t sim_layout;
extern uint32_t sim_cands[]; extern uint8_t sim_cand_count;
void sim_reset(void);
void sim_advance(uint32_t ms);
void sim_press(int key);
void sim_release(int key);
void sim_set_mode(hal_out_mode_t m);

static int fails = 0;
static void chk(const char *n, const char *got, const char *exp)
{
    if (strcmp(got, exp)) { printf("❌ %s\n   got=[%s]\n   exp=[%s]\n", n, got, exp); fails++; }
    else printf("✅ %s\n", n);
}
static void chki(const char *n, long got, long exp)
{
    if (got != exp) { printf("❌ %s got=%ld exp=%ld\n", n, got, exp); fails++; }
    else printf("✅ %s\n", n);
}

static cuime_app_t app;

/* 키를 눌렀다 뗀다 (스캔 경로를 통과) */
static void tap(int key)
{
    sim_press(key);   cuime_app_poll(&app);
    sim_advance(30);
    sim_release(key); cuime_app_poll(&app);
    sim_advance(10);
}
static void hold(int key, uint32_t ms)
{
    sim_press(key); cuime_app_poll(&app);
    for (uint32_t t = 0; t < ms; t += 20) { sim_advance(20); cuime_app_poll(&app); }
    sim_release(key); cuime_app_poll(&app);
    sim_advance(10);
}
static void settle(void)   /* 멀티탭 타임아웃 경과 */
{
    sim_advance(900);
    cuime_app_poll(&app);
}

static void fresh(void) { sim_reset(); cuime_app_init(&app); }

/* 데모 한자 사전 */
static uint8_t demo_hanja(uint32_t s, uint32_t *out, uint8_t max)
{
    if (s == 0xAD6D) {                    /* 국 */
        const uint32_t c[] = {0x570B, 0x5C40, 0x83CA};
        uint8_t n = 3 < max ? 3 : max;
        for (uint8_t i = 0; i < n; i++) out[i] = c[i];
        return n;
    }
    return 0;
}

int main(void)
{
    hal_board_init();

    /* ── 1. 유니코드 모드: 한글 조합 ── */
    fresh(); sim_set_mode(HAL_OUT_UNICODE_TEXT);
    /* 나무위키 = K5 K1 K2 K0 K0 K3 K2 K0 K3 K2 K1 K4 K4 K1 */
    int seq1[] = {CUIME_KEY_5,CUIME_KEY_1,CUIME_KEY_2,CUIME_KEY_0,CUIME_KEY_0,
                  CUIME_KEY_3,CUIME_KEY_2,CUIME_KEY_0,CUIME_KEY_3,CUIME_KEY_2,
                  CUIME_KEY_1,CUIME_KEY_4,CUIME_KEY_4,CUIME_KEY_1};
    for (unsigned i = 0; i < sizeof seq1/sizeof *seq1; i++) tap(seq1[i]);
    settle();
    { cuime_evlist_t ev; cuime_flush(&app.core, &ev);
      for (uint8_t i=0;i<ev.count;i++)
        if (ev.ev[i].type==CUIME_EV_COMMIT) hal_out_unicode(ev.ev[i].codepoint); }
    chk("유니코드 조합: 나무위키", sim_out, "나무위키");

    /* ── 2. 겹받침 (지연 확정 경로) ── */
    fresh(); sim_set_mode(HAL_OUT_UNICODE_TEXT);
    int seq2[] = {CUIME_KEY_8,CUIME_KEY_1,CUIME_KEY_2,CUIME_KEY_5,CUIME_KEY_5,
                  CUIME_KEY_0,CUIME_KEY_0};
    for (unsigned i = 0; i < sizeof seq2/sizeof *seq2; i++) tap(seq2[i]);
    settle();
    { cuime_evlist_t ev; cuime_flush(&app.core, &ev);
      for (uint8_t i=0;i<ev.count;i++)
        if (ev.ev[i].type==CUIME_EV_COMMIT) hal_out_unicode(ev.ev[i].codepoint); }
    chk("겹받침 삶 (지연확정)", sim_out, "삶");

    /* ── 3. 롱프레스 = 순환 마지막 ── */
    fresh(); sim_set_mode(HAL_OUT_UNICODE_TEXT);
    hold(CUIME_KEY_7, 400);              /* ㅃ */
    tap(CUIME_KEY_1); tap(CUIME_KEY_2);  /* ㅏ */
    settle();
    { cuime_evlist_t ev; cuime_flush(&app.core, &ev);
      for (uint8_t i=0;i<ev.count;i++)
        if (ev.ev[i].type==CUIME_EV_COMMIT) hal_out_unicode(ev.ev[i].codepoint); }
    chk("롱프레스 빠", sim_out, "빠");

    /* ── 4. 두벌식 HID 출력 ── */
    fresh(); sim_set_mode(HAL_OUT_HID_DUBEOLSIK);
    tap(CUIME_KEY_4); tap(CUIME_KEY_1); tap(CUIME_KEY_2);   /* 가 */
    settle();
    { cuime_evlist_t ev; cuime_flush(&app.core, &ev);
      for (uint8_t i=0;i<ev.count;i++)
        if (ev.ev[i].type==CUIME_EV_COMMIT) {
          uint32_t c,v,j; cuime_decompose(ev.ev[i].codepoint,&c,&v,&j);
          char a,b; cuime_to_dubeolsik(c,&a,&b); hal_out_ascii(a,false);
          cuime_to_dubeolsik(v,&a,&b); hal_out_ascii(a,false);
        } }
    chk("두벌식 HID: 가 -> rk", sim_out, "rk");

    /* ── 5. 멀티탭 타임아웃 (앱 타이머가 코어에 주입) ── */
    fresh(); sim_set_mode(HAL_OUT_UNICODE_TEXT);
    tap(CUIME_KEY_4);                    /* ㄱ */
    settle();                            /* 타임아웃 -> 세션 종료 */
    tap(CUIME_KEY_4);                    /* 다시 ㄱ (ㅋ 아님) */
    settle();
    { cuime_evlist_t ev; cuime_flush(&app.core, &ev);
      for (uint8_t i=0;i<ev.count;i++)
        if (ev.ev[i].type==CUIME_EV_COMMIT) hal_out_unicode(ev.ev[i].codepoint); }
    chk("타임아웃 후 재입력 ㄱㄱ", sim_out, "ㄱㄱ");

    /* 타임아웃 없이 연타하면 순환 */
    fresh(); sim_set_mode(HAL_OUT_UNICODE_TEXT);
    tap(CUIME_KEY_4); tap(CUIME_KEY_4);
    settle();
    { cuime_evlist_t ev; cuime_flush(&app.core, &ev);
      for (uint8_t i=0;i<ev.count;i++)
        if (ev.ev[i].type==CUIME_EV_COMMIT) hal_out_unicode(ev.ev[i].codepoint); }
    chk("연타 순환 ㅋ", sim_out, "ㅋ");

    /* ── 6. ▶ 조합 확정 ── */
    fresh(); sim_set_mode(HAL_OUT_UNICODE_TEXT);
    tap(CUIME_KEY_4); tap(CUIME_KEY_RIGHT);
    tap(CUIME_KEY_4); tap(CUIME_KEY_1); tap(CUIME_KEY_2);
    settle();
    { cuime_evlist_t ev; cuime_flush(&app.core, &ev);
      for (uint8_t i=0;i<ev.count;i++)
        if (ev.ev[i].type==CUIME_EV_COMMIT) hal_out_unicode(ev.ev[i].codepoint); }
    chk("▶ 확정 ㄱ가", sim_out, "ㄱ가");

    /* ── 7. 漢 + ▶ 코드 = 레이아웃 전환 ── */
    fresh();
    sim_press(CUIME_KEY_HANJA); cuime_app_poll(&app); sim_advance(50);
    sim_press(CUIME_KEY_RIGHT); cuime_app_poll(&app); sim_advance(20);
    sim_release(CUIME_KEY_RIGHT); cuime_app_poll(&app);
    sim_release(CUIME_KEY_HANJA); cuime_app_poll(&app);
    chki("코드 -> 영어 레이아웃", (long)app.core.layout, CUIME_LAYOUT_ENGLISH);
    chki("코드 후 후보/팔레트 없음", (long)(app.cand_count + app.palette_open), 0);

    /* 영문 E.161 멀티탭 */
    sim_out_len = 0; sim_out[0] = 0;
    tap(CUIME_KEY_7); tap(CUIME_KEY_7); tap(CUIME_KEY_7); tap(CUIME_KEY_7);
    chk("영문 PQRS 4탭 -> s", sim_out, "s");

    /* 영문 롱프레스 = 순환 마지막 */
    sim_out_len = 0; sim_out[0] = 0;
    hold(CUIME_KEY_9, 400);
    chk("영문 롱프레스 -> z", sim_out, "z");

    /* 숫자 레이아웃 */
    sim_press(CUIME_KEY_HANJA); cuime_app_poll(&app); sim_advance(50);
    sim_press(CUIME_KEY_RIGHT); cuime_app_poll(&app); sim_advance(20);
    sim_release(CUIME_KEY_RIGHT); cuime_app_poll(&app);
    sim_release(CUIME_KEY_HANJA); cuime_app_poll(&app);
    chki("코드 -> 숫자 레이아웃", (long)app.core.layout, CUIME_LAYOUT_NUMBER);
    sim_out_len = 0; sim_out[0] = 0;
    tap(CUIME_KEY_0); tap(CUIME_KEY_1); tap(CUIME_KEY_0);
    chk("숫자 010", sim_out, "010");

    /* 3회 순환 -> 한글 복귀 */
    sim_press(CUIME_KEY_HANJA); cuime_app_poll(&app); sim_advance(50);
    sim_press(CUIME_KEY_RIGHT); cuime_app_poll(&app); sim_advance(20);
    sim_release(CUIME_KEY_RIGHT); cuime_app_poll(&app);
    sim_release(CUIME_KEY_HANJA); cuime_app_poll(&app);
    chki("3회 순환 -> 한글", (long)app.core.layout, CUIME_LAYOUT_HANGUL);

    /* ── 8. 한자 변환 ── */
    fresh(); sim_set_mode(HAL_OUT_UNICODE_TEXT);
    app.hanja_lookup = demo_hanja;
    tap(CUIME_KEY_4); tap(CUIME_KEY_2); tap(CUIME_KEY_3);   /* 고 */
    tap(CUIME_KEY_4);                                        /* +ㄱ = 국? */
    /* '국' 만들기: ㄱ ㅜ ㄱ = K4 K3 K2 K4 */
    fresh(); sim_set_mode(HAL_OUT_UNICODE_TEXT);
    app.hanja_lookup = demo_hanja;
    tap(CUIME_KEY_4); tap(CUIME_KEY_3); tap(CUIME_KEY_2); tap(CUIME_KEY_4);
    settle();
    chki("조합 결과 = 국", (long)cuime_preedit(&app.core), 0xAD6D);
    sim_press(CUIME_KEY_HANJA); cuime_app_poll(&app); sim_advance(50);
    sim_release(CUIME_KEY_HANJA); cuime_app_poll(&app);
    chki("한자 후보 3개", (long)app.cand_count, 3);
    chki("첫 후보 國", (long)app.cand[0], 0x570B);
    /* 방향키로 후보 이동 */
    tap(CUIME_KEY_RIGHT);
    chki("후보 이동", (long)app.cand_index, 1);

    /* ── 9. M1 전송 가드 (docs/00 §7) ── */
    fresh(); sim_set_mode(HAL_OUT_UNICODE_TEXT);
    tap(CUIME_KEY_4); tap(CUIME_KEY_1); tap(CUIME_KEY_2);
    settle();
    sim_out_len = 0; sim_out[0] = 0;
    tap(CUIME_KEY_BACK);                 /* ⌫ */
    tap(CUIME_KEY_ENTER);                /* 300ms 내 ↵ -> 차단 */
    chki("M1 엔터 차단 (개행 없음)", (long)(strchr(sim_out, '\n') != NULL), 0);
    sim_advance(400); cuime_app_poll(&app);
    tap(CUIME_KEY_ENTER);                /* 가드 만료 후 통과 */
    chki("M1 만료 후 통과", (long)(strchr(sim_out, '\n') != NULL), 1);

    /* ── 10. 백스페이스 자모 역추적 ── */
    fresh(); sim_set_mode(HAL_OUT_UNICODE_TEXT);
    tap(CUIME_KEY_5); tap(CUIME_KEY_1); tap(CUIME_KEY_2);   /* 나 */
    tap(CUIME_KEY_BACK);                                     /* -> 니 */
    chki("나⌫ -> 니", (long)cuime_preedit(&app.core), 0xB2C8);

    printf("\n%s\n", fails == 0
        ? "✅ 펌웨어 통합 검증 전 항목 통과 (core + app + HAL)"
        : "❌ 실패 있음");
    return fails ? 1 : 0;
}
