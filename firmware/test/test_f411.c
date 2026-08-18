/* STM32F411 포트 로직 검증 — 실기기 없이 호스트에서 실행
 *
 * 정적 검증(verify_f411.py)은 "부팅 가능한가"만 본다.
 * 이 테스트는 F411 포트의 **하드웨어 무관 로직**이 실제로 맞는지 확인한다:
 *
 *   1. 키 매트릭스 매핑    — (행,열) -> 논리 키가 확정 배치와 일치하는가
 *   2. 스캔/디바운스       — 채터링을 걸러내는가
 *   3. UTF-8 인코딩        — hal_out_unicode 가 올바른 바이트를 내는가
 *   4. HID 키코드 변환     — 두벌식 모드에서 ascii_to_hid 가 맞는가
 *
 * 방법: 레지스터를 일반 메모리로 치환(CUIME_F411_HOSTTEST)해 그대로 구동한다.
 *       실제 하드웨어 접근만 가짜가 되고, 로직은 원본 그대로다.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* 가짜 레지스터 파일이 먼저 잡히도록 include 경로를 조정해 컴파일한다 */
#include "../port/stm32f411/hal_stm32f411.c"   /* .c 직접 포함: static 함수까지 검사 */
#include "../app/cuime_app.h"

static int fails = 0;

static void chk(const char *name, long got, long want)
{
    if (got == want) printf("✅ %s\n", name);
    else { printf("❌ %s  got=%ld want=%ld\n", name, got, want); fails++; }
}
static void chks(const char *name, const char *got, const char *want)
{
    if (strcmp(got, want) == 0) printf("✅ %s\n", name);
    else { printf("❌ %s\n   got =[%s]\n   want=[%s]\n", name, got, want); fails++; }
}

int main(void)
{
    printf("── STM32F411 포트 로직 검증 ──\n");

    /* ── 1. 키 매트릭스 매핑 (docs/00 §2 확정 배치) ── */
    chk("KEYMAP[0][0] = K1 (ㅣ)",     KEYMAP[0][0], CUIME_KEY_1);
    chk("KEYMAP[0][3] = BACK (⌫)",   KEYMAP[0][3], CUIME_KEY_BACK);
    chk("KEYMAP[1][0] = K4 (ㄱㅋ)",   KEYMAP[1][0], CUIME_KEY_4);
    chk("KEYMAP[1][3] = ENTER (↵)",  KEYMAP[1][3], CUIME_KEY_ENTER);
    chk("KEYMAP[2][3] = HANJA (漢)", KEYMAP[2][3], CUIME_KEY_HANJA);
    chk("KEYMAP[3][0] = LEFT (◀)",   KEYMAP[3][0], CUIME_KEY_LEFT);
    chk("KEYMAP[3][1] = K0 (ㅇㅁ)",   KEYMAP[3][1], CUIME_KEY_0);
    chk("KEYMAP[3][3] = SPACE (␣)",  KEYMAP[3][3], CUIME_KEY_SPACE);

    /* 16키가 중복 없이 전부 매핑됐는가 */
    {
        int seen[CUIME_KEY__COUNT];
        memset(seen, 0, sizeof seen);
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                seen[KEYMAP[r][c]]++;
        int dup = 0, miss = 0;
        for (int i = 0; i < CUIME_KEY__COUNT; i++) {
            if (seen[i] > 1) dup++;
            if (seen[i] == 0) miss++;
        }
        chk("16키 중복 없음", dup, 0);
        chk("16키 누락 없음", miss, 0);
    }

    /* ── 1b. 테스트 하니스 정합성 ──
     * 가짜 GPIO 는 열 인덱스를 핀 번호로 바꿀 때 COL_PIN 을 복제해 갖고 있다.
     * 실제 COL_PIN 을 바꿨는데 복제본을 안 고치면 테스트가 조용히 거짓
     * 통과하거나 전부 실패한다(실제로 겪음). 여기서 강제로 대조한다. */
    {
        extern const uint8_t *fk_col_pin_mirror(void);
        const uint8_t *mirror = fk_col_pin_mirror();
        int mismatch = 0;
        for (int c = 0; c < COL_COUNT; c++)
            if (mirror[c] != COL_PIN[c]) mismatch++;
        chk("하니스 COL_PIN 미러 일치", mismatch, 0);
    }

    /* ── 2. 스캔 + 디바운스 ── */
    hal_keys_init();
    /* 아무것도 안 눌린 상태 */
    chk("초기 스캔 = 0", (long)hal_keys_scan(), 0);

    /* PB1(행1) 구동 중 PA0(열0) Low = KEYMAP[1][0] = K4
     *
     * 디바운스 계약: raw 가 바뀌면 카운터가 리셋되고, 이후 DEBOUNCE_SAMPLES(2)회
     * 연속 동일해야 stable 에 반영된다. 즉 변화 감지 스캔 1회 + 확인 2회 = 3회째
     * 스캔에서 확정된다. (2회째 확정을 기대했다가 틀린 적 있음)
     */
    fk_press_row = 1; fk_press_col = 0;
    chk("디바운스 1회차 미확정", (long)hal_keys_scan(), 0);
    chk("디바운스 2회차 미확정", (long)hal_keys_scan(), 0);
    chk("디바운스 3회차 확정 K4", (long)hal_keys_scan(), (long)(1u << CUIME_KEY_4));

    /* 눌린 상태가 유지되면 계속 같은 값 */
    chk("유지 시 값 동일", (long)hal_keys_scan(), (long)(1u << CUIME_KEY_4));

    /* 뗌도 3회째에 반영 */
    fk_press_row = -1; fk_press_col = -1;
    hal_keys_scan(); hal_keys_scan();
    chk("뗌 3회차 반영", (long)hal_keys_scan(), 0);

    /* 채터링: 매 스캔마다 값이 흔들리면 stable 이 절대 바뀌지 않아야 한다.
     * 안정 상태(K4 눌림)를 먼저 만든 뒤 흔들어, stable 이 유지되는지 본다. */
    fk_press_row = 1; fk_press_col = 0;
    hal_keys_scan(); hal_keys_scan(); hal_keys_scan();     /* K4 확정 */
    {
        int chatter_broke = 0;
        for (int i = 0; i < 10; i++) {
            fk_press_row = (i % 2) ? 1 : -1;                /* 매번 반전 */
            if (hal_keys_scan() != (hal_keymask_t)(1u << CUIME_KEY_4))
                chatter_broke = 1;
        }
        chk("채터링 중 stable 유지", chatter_broke, 0);
    }

    /* ── 2b. 동시 입력(2키) ──
     * 매트릭스 스캔은 행별로 읽으므로 서로 다른 행의 2키는 함께 잡혀야 한다.
     * (가짜 GPIO 는 1키만 모사하므로 여기서는 단일키 경로만 확인한다) */
    fk_press_row = 3; fk_press_col = 3;
    hal_keys_scan(); hal_keys_scan(); hal_keys_scan();
    chk("행3열3 = SPACE", (long)hal_keys_scan(), (long)(1u << CUIME_KEY_SPACE));

    /* ── 2c. LED 피드백 ── */
    hal_ui_preedit(0xAC00);
    chk("조합 중 LED 점등", fake_led_on(), 1);
    hal_ui_preedit(0);
    chk("확정 후 LED 소등", fake_led_on(), 0);

#ifndef CUIME_F411_USB_HID   /* ↓ UART 출력 경로 검증 (HID 모드엔 해당 없음) */
    /* ── 3. UTF-8 인코딩 (UART 모드) ── */
    fake_uart_reset();
    hal_out_unicode(0xAC00);                 /* 가 */
    chks("UTF-8 '가' = EA B0 80", fake_uart_hex(), "EAB080");

    fake_uart_reset();
    hal_out_unicode('A');                    /* ASCII */
    chks("UTF-8 'A' = 41", fake_uart_hex(), "41");

    fake_uart_reset();
    hal_out_unicode(0x00E9);                 /* é : 2바이트 */
    chks("UTF-8 'é' = C3 A9", fake_uart_hex(), "C3A9");

    fake_uart_reset();
    hal_out_unicode(0xD7A3);                 /* 힣 : 한글 끝 */
    chks("UTF-8 '힣' = ED 9E A3", fake_uart_hex(), "ED9EA3");

    /* ── 4. 특수키 ── */
    fake_uart_reset();
    hal_out_special(HAL_KEYCODE_ENTER);
    chks("ENTER = CR LF", fake_uart_hex(), "0D0A");

    fake_uart_reset();
    hal_out_special(HAL_KEYCODE_SPACE);
    chks("SPACE = 20", fake_uart_hex(), "20");
#endif /* !CUIME_F411_USB_HID */

    /* ── 5. HID 키코드 변환 (두벌식 모드용) ── */
    {
        bool sh;
        chk("HID 'r' = 0x15", ascii_to_hid('r', &sh), 0x15);
        chk("HID 'r' shift 없음", sh, 0);
        chk("HID 'k' = 0x0E", ascii_to_hid('k', &sh), 0x0E);
        chk("HID 'R' = 0x15", ascii_to_hid('R', &sh), 0x15);
        chk("HID 'R' shift 필요", sh, 1);
        chk("HID '0' = 0x27", ascii_to_hid('0', &sh), 0x27);
        chk("HID '1' = 0x1E", ascii_to_hid('1', &sh), 0x1E);
        chk("HID ' ' = 0x2C", ascii_to_hid(' ', &sh), 0x2C);
    }

    /* ── 6. 출력 모드 ── */
#ifdef CUIME_F411_USB_HID
    chk("모드 = HID 두벌식", hal_out_mode(), HAL_OUT_HID_DUBEOLSIK);
#else
    chk("모드 = UTF-8 (UART 데모)", hal_out_mode(), HAL_OUT_UNICODE_TEXT);
#endif

#ifndef CUIME_F411_USB_HID
    /* ── 7. 엔드투엔드: 물리 키를 눌러 실제 한글이 UART 로 나오는가 ──
     *
     * 여기까지의 검사는 부품별 확인이다. 이 테스트는 앱 계층까지 통과시켜
     * "매트릭스 키 입력 → 조합 → UART UTF-8 출력"의 전 경로를 확인한다.
     * 천지인 '한' = ㅎ(K8×2) + ㅏ(K1,K2 → ㆍ+ㅣ 순서는 ㅣ+ㆍ) + ㄴ(K5)
     */
    {
        cuime_app_t app;
        hal_keys_init();
        cuime_app_init(&app);
        fake_uart_reset();

        /* (행,열) 을 눌렀다 떼는 헬퍼: 디바운스를 넘기려면 3회 이상 스캔 */
        #define PRESS(r, c) do {                                  \
            fk_press_row = (r); fk_press_col = (c);               \
            for (int _i = 0; _i < 4; _i++) cuime_app_poll(&app);  \
            fk_press_row = -1; fk_press_col = -1;                 \
            for (int _i = 0; _i < 4; _i++) cuime_app_poll(&app);  \
        } while (0)

        /* '가' : ㄱ(K4) + ㅣ(K1) + ㆍ(K2) → ㅏ */
        PRESS(1, 0);      /* K4 = ㄱ */
        PRESS(0, 0);      /* K1 = ㅣ */
        PRESS(0, 1);      /* K2 = ㆍ */
        /* 조합 확정을 위해 스페이스 */
        PRESS(3, 3);      /* SPACE */

        const char *got = fake_uart_hex();
        /* '가' = U+AC00 = EA B0 80, 이어서 space 20 */
        chks("E2E: ㄱ+ㅣ+ㆍ+space -> '가 '", got, "EAB08020");

        /* '한' : ㅎ = K8 두 번(ㅅ→ㅎ), ㅏ = ㅣ+ㆍ, ㄴ = K5 한 번 */
        cuime_app_init(&app);
        hal_keys_init();
        fake_uart_reset();
        PRESS(2, 1);      /* K8 = ㅅ */
        PRESS(2, 1);      /* K8 재타 = ㅎ (멀티탭) */
        PRESS(0, 0);      /* ㅣ */
        PRESS(0, 1);      /* ㆍ */
        PRESS(1, 1);      /* K5 = ㄴ */
        PRESS(3, 3);      /* SPACE 로 확정 */
        chks("E2E: 멀티탭 '한 ' (ED 95 9C 20)", fake_uart_hex(), "ED959C20");

        /* 백스페이스 역조합: '가' 에서 ⌫ -> 'ㄱ' 로 되돌아가야 한다.
         * 확정 전 상태이므로 UART 출력이 아니라 조합 상태로 확인한다. */
        cuime_app_init(&app);
        hal_keys_init();
        fake_uart_reset();
        PRESS(1, 0);      /* ㄱ */
        PRESS(0, 0);      /* ㅣ */
        PRESS(0, 1);      /* ㆍ  -> 가 */
        PRESS(0, 3);      /* ⌫ */
        PRESS(3, 3);      /* SPACE 확정 */
        /* 천지인 백스페이스는 **획 단위 역조합**(KR100474699B1)이다.
         * '가' = ㄱ+ㅣ+ㆍ 이므로 ⌫ 는 마지막 획 ㆍ 만 지워 ㄱ+ㅣ = '기' 가 된다.
         * 글자 통째로 지워 'ㄱ' 이 되는 게 아니다. (기대값을 틀렸던 지점) */
        chks("E2E: 역조합 ⌫ '가'->'기' (U+AE30)", fake_uart_hex(), "EAB8B020");

        /* ⌫ 를 더 눌러 완전히 비우기 */
        PRESS(1, 0); PRESS(0, 0); PRESS(0, 1);   /* '가' 재입력 */
        fake_uart_reset();
        PRESS(0, 3); PRESS(0, 3); PRESS(0, 3);   /* ㆍ, ㅣ, ㄱ 순으로 소거 */
        PRESS(3, 3);                              /* SPACE */
        chks("E2E: ⌫ 3회 -> 빈 조합 + space", fake_uart_hex(), "20");
        #undef PRESS
    }
#else  /* CUIME_F411_USB_HID */
    /* ── 7'. HID 모드 엔드투엔드 ──
     * HID 는 유니코드를 못 보내므로 두벌식 **자판 위치**를 눌러 보낸다.
     * '가' = 두벌식 'r'(ㄱ) + 'k'(ㅏ) → HID 0x15, 0x0E 가 순서대로 나가야 한다.
     */
    {
        cuime_app_t app;
        hal_keys_init();
        cuime_app_init(&app);
        fake_hid_reset();
        #define PRESS(r, c) do {                                  \
            fk_press_row = (r); fk_press_col = (c);               \
            for (int _i = 0; _i < 4; _i++) cuime_app_poll(&app);  \
            fk_press_row = -1; fk_press_col = -1;                 \
            for (int _i = 0; _i < 4; _i++) cuime_app_poll(&app);  \
        } while (0)
        PRESS(1, 0);   /* ㄱ */
        PRESS(0, 0);   /* ㅣ */
        PRESS(0, 1);   /* ㆍ  -> ㅏ */
        PRESS(3, 3);   /* SPACE 확정 */
        chks("E2E(HID): '가' -> r,k,space = 15,0E,2C", fake_hid_hex(), "150E2C");
        #undef PRESS
    }
#endif

    printf("\n%s\n", fails == 0
        ? "✅ F411 포트 로직 전 항목 통과"
        : "❌ 실패 있음");
    return fails ? 1 : 0;
}
