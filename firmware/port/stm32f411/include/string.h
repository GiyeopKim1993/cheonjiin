/* freestanding 용 <string.h> 대체
 *
 * 베어메탈 빌드에는 libc 헤더가 없다. 앱/코어가 쓰는 선언만 제공한다.
 * 구현은 freestanding_libc.c 에 있다.
 * (-Iport/stm32f411/include 로 이 헤더가 먼저 잡히게 한다)
 */
#ifndef CUIME_FREESTANDING_STRING_H
#define CUIME_FREESTANDING_STRING_H
#include <stddef.h>
void  *memset(void *dst, int c, size_t n);
void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
#endif
