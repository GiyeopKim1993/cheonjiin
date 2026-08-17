/* 천지인 펌웨어 조합 테이블 — 자동 생성 (tools/gen_firmware_tables.py)
 * 직접 수정 금지. 참조 구현(tools/engine.py)에서 생성됨.
 *
 * 모든 테이블은 const 이므로 MCU 플래시(.rodata)에 놓인다. RAM 소비 0.
 */
#ifndef CUIME_TABLES_H
#define CUIME_TABLES_H

#include <stdint.h>

#define CUIME_CHO_COUNT  19
#define CUIME_JUNG_COUNT 21
#define CUIME_JONG_COUNT 28

/* 초성 19 (조합용 인덱스 순서) */
static const uint16_t CUIME_CHO[CUIME_CHO_COUNT] = {
    0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141, 0x3142, 0x3143, 0x3145, 0x3146, 0x3147, 0x3148, 0x3149, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E
};

/* 중성 21 */
static const uint16_t CUIME_JUNG[CUIME_JUNG_COUNT] = {
    0x314F, 0x3150, 0x3151, 0x3152, 0x3153, 0x3154, 0x3155, 0x3156, 0x3157, 0x3158, 0x3159, 0x315A, 0x315B, 0x315C, 0x315D, 0x315E, 0x315F, 0x3160, 0x3161, 0x3162, 0x3163
};

/* 종성 28 (0 = 받침 없음) */
static const uint16_t CUIME_JONG[CUIME_JONG_COUNT] = {
    0x0000, 0x3131, 0x3132, 0x3133, 0x3134, 0x3135, 0x3136, 0x3137, 0x3139, 0x313A, 0x313B, 0x313C, 0x313D, 0x313E, 0x313F, 0x3140, 0x3141, 0x3142, 0x3144, 0x3145, 0x3146, 0x3147, 0x3148, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E
};

#define CUIME_VSTATE_COUNT 24
#define CUIME_VS_NONE 0
#define CUIME_VS_DOT  22   /* 미완성: ㆍ */
#define CUIME_VS_DOT2 23  /* 미완성: ㆍㆍ */

/* 모음 상태 -> 확정 중성 인덱스(0-base). 0xFF = 미완성(화면 미출력) */
static const uint8_t CUIME_VSTATE_JUNG[CUIME_VSTATE_COUNT] = {
    0xFF, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 0xFF, 0xFF
};

/* 전이표: [현재상태][키(0=ㅣ,1=ㆍ,2=ㅡ)] -> 다음상태. 0xFF = 전이 없음 */
static const uint8_t CUIME_VTRANS[CUIME_VSTATE_COUNT][3] = {
    {       21, 22, 19 },   /* (none) */
    {       2, 3, 0xFF },   /* ㅏ */
    {    0xFF, 4, 0xFF },   /* ㅐ */
    {       4, 1, 0xFF },   /* ㅑ */
    { 0xFF, 0xFF, 0xFF },   /* ㅒ */
    {    6, 0xFF, 0xFF },   /* ㅓ */
    {    0, 0xFF, 0xFF },   /* ㅔ */
    {    8, 0xFF, 0xFF },   /* ㅕ */
    { 0xFF, 0xFF, 0xFF },   /* ㅖ */
    {   12, 0xFF, 0xFF },   /* ㅗ */
    {   11, 0xFF, 0xFF },   /* ㅘ */
    { 0xFF, 0xFF, 0xFF },   /* ㅙ */
    {   0xFF, 10, 0xFF },   /* ㅚ */
    { 0xFF, 0xFF, 0xFF },   /* ㅛ */
    {     17, 18, 0xFF },   /* ㅜ */
    {   16, 0xFF, 0xFF },   /* ㅝ */
    { 0xFF, 0xFF, 0xFF },   /* ㅞ */
    {   0xFF, 15, 0xFF },   /* ㅟ */
    {   15, 0xFF, 0xFF },   /* ㅠ */
    {     20, 14, 0xFF },   /* ㅡ */
    { 0xFF, 0xFF, 0xFF },   /* ㅢ */
    {    0xFF, 1, 0xFF },   /* ㅣ */
    {         5, 23, 9 },   /* ㆍ */
    {        7, 22, 13 },   /* ㆍㆍ */
};

/* 백스페이스 역추적: 상태 -> 이전 상태. 0xFF = 없음
 * 참조 구현(engine._PREV)을 그대로 가져온다. 순회 순서에 의존하므로
 * 재구현하면 어긋난다(실제로 겪은 버그). */
static const uint8_t CUIME_VPREV[CUIME_VSTATE_COUNT] = {
    0xFF, 21, 1, 1, 3, 22, 5, 23, 7, 22, 12, 10, 9, 23, 19, 18, 15, 14, 14, 0, 19, 0, 0, 22
};

#define CUIME_CKEY_COUNT 7
#define CUIME_CYCLE_MAX 3

/* 자음 키별 순환열 (유니코드 자모). 0 = 빈 슬롯 */
static const uint16_t CUIME_CYCLE[CUIME_CKEY_COUNT][CUIME_CYCLE_MAX] = {
    { 0x3131, 0x314B, 0x3132 },   /* K4: ㄱ→ㅋ→ㄲ */
    { 0x3134, 0x3139, 0x0000 },   /* K5: ㄴ→ㄹ */
    { 0x3137, 0x314C, 0x3138 },   /* K6: ㄷ→ㅌ→ㄸ */
    { 0x3142, 0x314D, 0x3143 },   /* K7: ㅂ→ㅍ→ㅃ */
    { 0x3145, 0x314E, 0x3146 },   /* K8: ㅅ→ㅎ→ㅆ */
    { 0x3148, 0x314A, 0x3149 },   /* K9: ㅈ→ㅊ→ㅉ */
    { 0x3147, 0x3141, 0x0000 },   /* K0: ㅇ→ㅁ */
};

static const uint8_t CUIME_CYCLE_LEN[CUIME_CKEY_COUNT] = {
    3, 2, 3, 3, 3, 3, 2
};

/* 롱프레스 = 순환열 마지막 항목 (docs/00 §3.2) */
static const uint16_t CUIME_LONGPRESS[CUIME_CKEY_COUNT] = {
    0x3132, 0x3139, 0x3138, 0x3143, 0x3146, 0x3149, 0x3141
};

#define CUIME_CLUSTER_COUNT 11
/* 겹받침: {합성 종성, 앞 자모, 뒤 자모} */
static const uint16_t CUIME_CLUSTER[CUIME_CLUSTER_COUNT][3] = {
    { 0x3133, 0x3131, 0x3145 },   /* ㄳ = ㄱ+ㅅ */
    { 0x3135, 0x3134, 0x3148 },   /* ㄵ = ㄴ+ㅈ */
    { 0x3136, 0x3134, 0x314E },   /* ㄶ = ㄴ+ㅎ */
    { 0x313A, 0x3139, 0x3131 },   /* ㄺ = ㄹ+ㄱ */
    { 0x313B, 0x3139, 0x3141 },   /* ㄻ = ㄹ+ㅁ */
    { 0x313C, 0x3139, 0x3142 },   /* ㄼ = ㄹ+ㅂ */
    { 0x313D, 0x3139, 0x3145 },   /* ㄽ = ㄹ+ㅅ */
    { 0x313E, 0x3139, 0x314C },   /* ㄾ = ㄹ+ㅌ */
    { 0x313F, 0x3139, 0x314D },   /* ㄿ = ㄹ+ㅍ */
    { 0x3140, 0x3139, 0x314E },   /* ㅀ = ㄹ+ㅎ */
    { 0x3144, 0x3142, 0x3145 },   /* ㅄ = ㅂ+ㅅ */
};

/* ── 두벌식 HID 매핑 ──
 * BT/USB HID 키보드는 유니코드가 아니라 키코드를 보낸다.
 * 따라서 기기가 조합한 자모를 두벌식 자판 위치로 변환해 전송하고,
 * 호스트의 한글 IME가 음절로 합친다. (docs/10 §4)
 * 값: ASCII 문자 (대문자 = Shift 필요). 0 = 매핑 없음.
 */
typedef struct { uint16_t jamo; char a; char b; } cuime_dubeol_t;
#define CUIME_DUBEOL_COUNT 51
static const cuime_dubeol_t CUIME_DUBEOL[CUIME_DUBEOL_COUNT] = {
    { 0x3131, 'r', 0 },   /* ㄱ */
    { 0x3132, 'R', 0 },   /* ㄲ */
    { 0x3133, 'r', 't' },   /* ㄳ */
    { 0x3134, 's', 0 },   /* ㄴ */
    { 0x3135, 's', 'w' },   /* ㄵ */
    { 0x3136, 's', 'g' },   /* ㄶ */
    { 0x3137, 'e', 0 },   /* ㄷ */
    { 0x3138, 'E', 0 },   /* ㄸ */
    { 0x3139, 'f', 0 },   /* ㄹ */
    { 0x313A, 'f', 'r' },   /* ㄺ */
    { 0x313B, 'f', 'a' },   /* ㄻ */
    { 0x313C, 'f', 'q' },   /* ㄼ */
    { 0x313D, 'f', 't' },   /* ㄽ */
    { 0x313E, 'f', 'x' },   /* ㄾ */
    { 0x313F, 'f', 'v' },   /* ㄿ */
    { 0x3140, 'f', 'g' },   /* ㅀ */
    { 0x3141, 'a', 0 },   /* ㅁ */
    { 0x3142, 'q', 0 },   /* ㅂ */
    { 0x3143, 'Q', 0 },   /* ㅃ */
    { 0x3144, 'q', 't' },   /* ㅄ */
    { 0x3145, 't', 0 },   /* ㅅ */
    { 0x3146, 'T', 0 },   /* ㅆ */
    { 0x3147, 'd', 0 },   /* ㅇ */
    { 0x3148, 'w', 0 },   /* ㅈ */
    { 0x3149, 'W', 0 },   /* ㅉ */
    { 0x314A, 'c', 0 },   /* ㅊ */
    { 0x314B, 'z', 0 },   /* ㅋ */
    { 0x314C, 'x', 0 },   /* ㅌ */
    { 0x314D, 'v', 0 },   /* ㅍ */
    { 0x314E, 'g', 0 },   /* ㅎ */
    { 0x314F, 'k', 0 },   /* ㅏ */
    { 0x3150, 'o', 0 },   /* ㅐ */
    { 0x3151, 'i', 0 },   /* ㅑ */
    { 0x3152, 'O', 0 },   /* ㅒ */
    { 0x3153, 'j', 0 },   /* ㅓ */
    { 0x3154, 'p', 0 },   /* ㅔ */
    { 0x3155, 'u', 0 },   /* ㅕ */
    { 0x3156, 'P', 0 },   /* ㅖ */
    { 0x3157, 'h', 0 },   /* ㅗ */
    { 0x3158, 'h', 'k' },   /* ㅘ */
    { 0x3159, 'h', 'o' },   /* ㅙ */
    { 0x315A, 'h', 'l' },   /* ㅚ */
    { 0x315B, 'y', 0 },   /* ㅛ */
    { 0x315C, 'n', 0 },   /* ㅜ */
    { 0x315D, 'n', 'j' },   /* ㅝ */
    { 0x315E, 'n', 'p' },   /* ㅞ */
    { 0x315F, 'n', 'l' },   /* ㅟ */
    { 0x3160, 'b', 0 },   /* ㅠ */
    { 0x3161, 'm', 0 },   /* ㅡ */
    { 0x3162, 'm', 'l' },   /* ㅢ */
    { 0x3163, 'l', 0 },   /* ㅣ */
};

#endif /* CUIME_TABLES_H */
