/* freestanding 최소 libc — 베어메탈 빌드용
 *
 * 코어(cuime.c)와 앱(cuime_app.c)은 <string.h> 의 memset/strlen 을 쓴다.
 * 베어메탈에는 libc 가 없으므로 여기서 제공한다.
 *
 * 앱 계층을 고치지 않는 이유: 앱은 "하드웨어 무관" 계층이라 호스트에서도
 * 그대로 컴파일돼야 하고, 표준 함수를 쓰는 게 정상이다. 없는 쪽(포트)이
 * 채우는 것이 계층 구조에 맞다.
 *
 * 컴파일러가 memset/memcpy 를 자동 생성(intrinsic)할 수도 있어
 * -ffreestanding 을 줘도 이 심볼들이 필요하다.
 */
#include <stddef.h>

void *memset(void *dst, int c, size_t n)
{
    unsigned char *p = (unsigned char *)dst;
    while (n--) *p++ = (unsigned char)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {                      /* 겹칠 때는 뒤에서부터 */
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = (const unsigned char *)a;
    const unsigned char *q = (const unsigned char *)b;
    while (n--) {
        if (*p != *q) return (int)*p - (int)*q;
        p++; q++;
    }
    return 0;
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

/* ── ARM EABI 헬퍼 ──
 * 컴파일러가 배열 0-초기화 등을 이 함수 호출로 바꾼다(-Os 에서 특히).
 * libgcc/newlib 이 없는 베어메탈에서는 직접 제공해야 링크된다.
 * 실제로 `uint8_t r[32] = {0};` 하나 때문에 __aeabi_memclr4 가 필요했다.
 */
void __aeabi_memclr(void *dst, size_t n)  { memset(dst, 0, n); }
void __aeabi_memclr4(void *dst, size_t n) { memset(dst, 0, n); }
void __aeabi_memclr8(void *dst, size_t n) { memset(dst, 0, n); }

void __aeabi_memcpy(void *d, const void *s, size_t n)  { memcpy(d, s, n); }
void __aeabi_memcpy4(void *d, const void *s, size_t n) { memcpy(d, s, n); }
void __aeabi_memcpy8(void *d, const void *s, size_t n) { memcpy(d, s, n); }

void __aeabi_memset(void *d, size_t n, int c)  { memset(d, c, n); }
void __aeabi_memset4(void *d, size_t n, int c) { memset(d, c, n); }
void __aeabi_memset8(void *d, size_t n, int c) { memset(d, c, n); }

void __aeabi_memmove(void *d, const void *s, size_t n)  { memmove(d, s, n); }
void __aeabi_memmove4(void *d, const void *s, size_t n) { memmove(d, s, n); }
void __aeabi_memmove8(void *d, const void *s, size_t n) { memmove(d, s, n); }
