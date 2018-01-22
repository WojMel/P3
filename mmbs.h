#include <stddef.h>

#ifndef mmbs_h__
#define mmbs_h__

extern void *malloc(size_t size);
extern void free(void *ptr);
extern void *calloc(size_t nmemb, size_t size);
extern void *realloc(void *ptr, size_t size);

#endif