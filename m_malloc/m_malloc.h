#ifndef m_malloc_h
#define m_malloc_h

#include <stddef.h>

void *m_malloc(size_t n_user);
void m_free(void *ptr);

#endif