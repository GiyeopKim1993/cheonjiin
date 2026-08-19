/* freestanding 용 <stdio.h> 최소 대체
 * TinyUSB 가 include 하지만 우리는 printf 계열을 쓰지 않는다.
 * (TU_LOG 를 끈 상태) 심볼만 있으면 된다. */
#ifndef CUIME_FREESTANDING_STDIO_H
#define CUIME_FREESTANDING_STDIO_H
#include <stddef.h>
int printf(const char *fmt, ...);
int snprintf(char *s, size_t n, const char *fmt, ...);
#endif
