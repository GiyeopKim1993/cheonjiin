/* 원시 키 이벤트 프로토콜 검증 — 실기기 없이 호스트에서
 *
 * 검사 대상:
 *   1. HID 리포트 디스크립터가 **파싱 가능한가**
 *      특히 macOS 가 요구하는 조건: Usage != 0, Collection(Application)
 *      (이 둘을 어겨 macOS 가 장치를 통째로 무시한 실제 사례가 있다)
 *   2. 리포트 인코딩/디코딩 왕복이 손실 없는가
 *   3. 하트비트 타임아웃 판정
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../hal/cuime_rawhid.h"
#include "../core/cuime.h"

static int fails = 0;

static void chk(const char *name, long got, long want)
{
    if (got == want) printf("✅ %s\n", name);
    else { printf("❌ %s  got=%ld want=%ld\n", name, got, want); fails++; }
}

/* ── HID 리포트 디스크립터 최소 파서 ──
 * OS 가 하는 일을 흉내 내 우리 디스크립터가 유효한지 본다.
 * "눈으로 봤을 때 맞아 보인다"는 검증이 아니다.
 */
typedef struct {
    uint32_t usage_page;
    uint32_t usage;          /* Collection 직전의 Usage */
    uint8_t  collection_type;
    int      collection_depth;
    int      balanced;       /* Collection/End 짝이 맞는가 */
    int      input_count, output_count;
    int      report_bytes_in, report_bytes_out;
} hid_parse_t;

static void parse_hid(const uint8_t *d, int n, hid_parse_t *o)
{
    memset(o, 0, sizeof *o);
    uint32_t cur_usage = 0, cur_size = 0, cur_count = 0;
    int depth = 0, maxdepth = 0;
    int seen_collection = 0;

    for (int i = 0; i < n; ) {
        uint8_t b = d[i++];
        int bSize = b & 0x03;
        if (bSize == 3) bSize = 4;
        int bType = (b >> 2) & 0x03;
        int bTag  = (b >> 4) & 0x0F;
        uint32_t val = 0;
        for (int k = 0; k < bSize && i < n; k++) val |= (uint32_t)d[i++] << (8 * k);

        if (bType == 1) {            /* Global */
            if (bTag == 0x0) o->usage_page = val;
            if (bTag == 0x7) cur_size = val;
            if (bTag == 0x9) cur_count = val;
        } else if (bType == 2) {     /* Local */
            if (bTag == 0x0) cur_usage = val;
        } else if (bType == 0) {     /* Main */
            if (bTag == 0xA) {       /* Collection */
                if (!seen_collection) {
                    o->usage = cur_usage;        /* 첫 Collection 직전 Usage */
                    o->collection_type = (uint8_t)val;
                    seen_collection = 1;
                }
                depth++;
                if (depth > maxdepth) maxdepth = depth;
            } else if (bTag == 0xC) {            /* End Collection */
                depth--;
            } else if (bTag == 0x8) {            /* Input */
                o->input_count++;
                o->report_bytes_in += (int)(cur_size * cur_count) / 8;
            } else if (bTag == 0x9) {            /* Output */
                o->output_count++;
                o->report_bytes_out += (int)(cur_size * cur_count) / 8;
            }
            cur_usage = 0;           /* Main item 이 Local 을 소비 */
        }
    }
    o->collection_depth = maxdepth;
    o->balanced = (depth == 0);
}

/* ── 리포트 인코딩 (디바이스가 하는 일) ── */
static void enc_key(uint8_t *r, cuime_key_t k, bool pressed, uint32_t ms)
{
    memset(r, 0, CUIME_RAW_REPORT_SIZE);
    r[0] = CUIME_RAW_PROTO_VER;
    r[1] = CUIME_RAW_EV_KEY;
    r[2] = (uint8_t)k;
    r[3] = pressed ? 1 : 0;
    r[4] = (uint8_t)(ms & 0xFF);
    r[5] = (uint8_t)((ms >> 8) & 0xFF);
    r[6] = (uint8_t)((ms >> 16) & 0xFF);
    r[7] = (uint8_t)((ms >> 24) & 0xFF);
}

/* ── 리포트 디코딩 (호스트 IME 가 하는 일) ── */
static bool dec_key(const uint8_t *r, cuime_key_t *k, bool *pressed, uint32_t *ms)
{
    if (r[0] != CUIME_RAW_PROTO_VER) return false;
    if (r[1] != CUIME_RAW_EV_KEY) return false;
    if (r[2] >= CUIME_KEY__COUNT) return false;      /* 범위 밖 키 거부 */
    *k = (cuime_key_t)r[2];
    *pressed = r[3] != 0;
    *ms = (uint32_t)r[4] | ((uint32_t)r[5] << 8)
        | ((uint32_t)r[6] << 16) | ((uint32_t)r[7] << 24);
    return true;
}

int main(void)
{
    printf("── 원시 키 이벤트 프로토콜 검증 ──\n");

    /* ── 1. HID 디스크립터 ── */
    static const uint8_t desc[] = { CUIME_RAW_HID_REPORT_DESC() };
    hid_parse_t p;
    parse_hid(desc, (int)sizeof desc, &p);

    chk("UsagePage = 0xFF60", (long)p.usage_page, 0xFF60);
    chk("Usage = 0x61 (0 이면 macOS 무시)", (long)p.usage, 0x61);
    chk("Collection = Application(0x01)", (long)p.collection_type, 0x01);
    chk("Collection/End 짝 맞음", p.balanced, 1);
    chk("Input 항목 1개", p.input_count, 1);
    chk("Output 항목 1개", p.output_count, 1);
    chk("IN 리포트 32바이트", p.report_bytes_in, CUIME_RAW_REPORT_SIZE);
    chk("OUT 리포트 32바이트", p.report_bytes_out, CUIME_RAW_REPORT_SIZE);

    /* macOS 실패 사례 재현: Usage 0 + 벤더 컬렉션이면 걸러져야 한다 */
    {
        static const uint8_t bad[] = {
            0x06, 0x00, 0xFF,   /* Usage Page 0xFF00 */
            0x09, 0x00,         /* Usage 0  ← 문제 */
            0xA1, 0xFF,         /* Collection (Vendor 0xFF) ← 문제 */
            0xC0
        };
        hid_parse_t q;
        parse_hid(bad, (int)sizeof bad, &q);
        int rejected = (q.usage == 0) || (q.collection_type != 0x01);
        chk("음성대조: Usage0+벤더컬렉션 = 위험으로 판정", rejected, 1);
    }

    /* ── 2. 인코딩/디코딩 왕복 ── */
    {
        uint8_t r[CUIME_RAW_REPORT_SIZE];
        int bad = 0;
        for (int k = 0; k < CUIME_KEY__COUNT; k++) {
            for (int pr = 0; pr <= 1; pr++) {
                uint32_t ms = 0xDEADBEEFu ^ (uint32_t)(k * 7919 + pr);
                enc_key(r, (cuime_key_t)k, pr != 0, ms);
                cuime_key_t k2; bool p2; uint32_t m2;
                if (!dec_key(r, &k2, &p2, &m2)) { bad++; continue; }
                if ((int)k2 != k || p2 != (pr != 0) || m2 != ms) bad++;
            }
        }
        chk("전체 키 왕복 무손실 (16키 x 2)", bad, 0);
    }

    /* 잘못된 리포트는 거부되어야 한다 */
    {
        uint8_t r[CUIME_RAW_REPORT_SIZE];
        cuime_key_t k; bool pr; uint32_t ms;

        enc_key(r, CUIME_KEY_1, true, 100);
        r[0] = 99;                       /* 프로토콜 버전 불일치 */
        chk("버전 불일치 거부", dec_key(r, &k, &pr, &ms), 0);

        enc_key(r, CUIME_KEY_1, true, 100);
        r[2] = CUIME_KEY__COUNT + 5;     /* 범위 밖 키 */
        chk("범위 밖 키 거부", dec_key(r, &k, &pr, &ms), 0);

        enc_key(r, CUIME_KEY_1, true, 100);
        r[1] = 0x7F;                     /* 모르는 메시지 종류 */
        chk("미지의 메시지 거부", dec_key(r, &k, &pr, &ms), 0);
    }

    /* ── 3. 하트비트 판정 ── */
    {
        /* last_ping 이후 HEARTBEAT_MS 지나면 끊긴 것 */
        uint32_t last = 10000;
        chk("하트비트 정상(1s 경과)",
            (10000u + 1000u - last) < CUIME_RAW_HEARTBEAT_MS, 1);
        chk("하트비트 만료(3.1s 경과)",
            (10000u + 3100u - last) < CUIME_RAW_HEARTBEAT_MS, 0);

        /* millis 오버플로 구간에서도 안전해야 한다 (부호 없는 뺄셈) */
        uint32_t near_max = 0xFFFFF000u;
        uint32_t after = near_max + 1000u;      /* 래핑 */
        chk("타이머 래핑 시에도 정상 판정",
            (uint32_t)(after - near_max) < CUIME_RAW_HEARTBEAT_MS, 1);
    }

    printf("\n%s\n", fails == 0
        ? "✅ 원시 키 이벤트 프로토콜 전 항목 통과"
        : "❌ 실패 있음");
    return fails ? 1 : 0;
}
