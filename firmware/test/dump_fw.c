/* 펌웨어 코어로 키 시퀀스를 재생해 결과 문자열을 덤프한다.
 * 참조 구현(Python/JS/Java/C++) 출력과 바이트 단위로 대조하기 위한 도구.
 *
 * 입력 형식 (참조 인코더 출력):  <기대문자열>\t<키 시퀀스 공백구분>
 * 출력 형식:                      <기대문자열>\t<펌웨어 조합 결과>
 */
#include "../core/cuime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void put_utf8(char **p, uint32_t cp)
{
    if (cp < 0x80) { *(*p)++ = (char)cp; }
    else if (cp < 0x800) {
        *(*p)++ = (char)(0xC0 | (cp >> 6));
        *(*p)++ = (char)(0x80 | (cp & 0x3F));
    } else {
        *(*p)++ = (char)(0xE0 | (cp >> 12));
        *(*p)++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *(*p)++ = (char)(0x80 | (cp & 0x3F));
    }
}

/* 토큰 -> 논리 키. LONG_ 접두는 롱프레스로 별도 처리 */
static int parse_key(const char *tok, cuime_key_t *out, int *is_long)
{
    *is_long = 0;
    if (!strncmp(tok, "LONG_K", 6)) {
        int d = tok[6] - '0';
        *out = (d == 0) ? CUIME_KEY_0 : (cuime_key_t)(CUIME_KEY_1 + d - 1);
        *is_long = 1;
        return 1;
    }
    if (!strcmp(tok, "KRIGHT")) { *out = CUIME_KEY_RIGHT; return 1; }
    if (!strcmp(tok, "KLEFT"))  { *out = CUIME_KEY_LEFT;  return 1; }
    if (!strcmp(tok, "SPACE"))  { *out = CUIME_KEY_SPACE; return 1; }
    if (!strcmp(tok, "ENTER"))  { *out = CUIME_KEY_ENTER; return 1; }
    if (!strcmp(tok, "BACK"))   { *out = CUIME_KEY_BACK;  return 1; }
    if (!strcmp(tok, "TIMEOUT")){ *out = CUIME_KEY_TIMEOUT; return 1; }
    if (tok[0] == 'K' && tok[1] >= '0' && tok[1] <= '9' && tok[2] == 0) {
        int d = tok[1] - '0';
        *out = (d == 0) ? CUIME_KEY_0 : (cuime_key_t)(CUIME_KEY_1 + d - 1);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s <in> <out>\n", argv[0]); return 2; }
    FILE *in = fopen(argv[1], "r");
    FILE *out = fopen(argv[2], "w");
    if (!in || !out) { fprintf(stderr, "file error\n"); return 2; }

    char line[1024];
    while (fgets(line, sizeof line, in)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        char *expected = line;
        char *keys = tab + 1;
        char *nl = strchr(keys, '\n'); if (nl) *nl = 0;

        cuime_t st; cuime_init(&st);
        cuime_evlist_t ev;
        char buf[512]; char *p = buf;

        
        for (char *tok = strtok(keys, " "); tok;
             tok = strtok(NULL, " ")) {
            cuime_key_t k; int is_long;
            if (!parse_key(tok, &k, &is_long)) continue;
            if (is_long) cuime_key_longpress(&st, k, &ev);
            else         cuime_key_down(&st, k, &ev);
            for (int i = 0; i < ev.count; i++) {
                if (ev.ev[i].type == CUIME_EV_COMMIT && ev.ev[i].codepoint)
                    put_utf8(&p, ev.ev[i].codepoint);
                else if (ev.ev[i].type == CUIME_EV_CONTROL &&
                         ev.ev[i].key == CUIME_KEY_SPACE)
                    *p++ = ' ';
            }
        }
        cuime_flush(&st, &ev);
        for (int i = 0; i < ev.count; i++)
            if (ev.ev[i].type == CUIME_EV_COMMIT && ev.ev[i].codepoint)
                put_utf8(&p, ev.ev[i].codepoint);
        *p = 0;

        fprintf(out, "%s\t%s\n", expected, buf);
    }
    fclose(in); fclose(out);
    return 0;
}
