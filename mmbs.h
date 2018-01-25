#include <stddef.h>
#include <stdbool.h>

#ifndef mmbs_h__
#define mmbs_h__

extern void *m_malloc(size_t size);
extern void m_free(void *ptr);
extern void *m_calloc(size_t nmemb, size_t size);
extern void *m_realloc(void *ptr, size_t size);

extern bool mm_print_tree(size_t i);
extern bool mm_print_tree_bin(size_t i);
extern void mm_startup();
#endif