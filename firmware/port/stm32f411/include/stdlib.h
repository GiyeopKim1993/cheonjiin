/* freestanding 용 <stdlib.h> 최소 대체 */
#ifndef CUIME_FREESTANDING_STDLIB_H
#define CUIME_FREESTANDING_STDLIB_H
#include <stddef.h>
void *malloc(size_t n);
void  free(void *p);
void  abort(void);
#endif
