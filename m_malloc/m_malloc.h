#ifndef my_malloc
#define my_malloc
#include <stddef.h>

void *m_malloc(size_t n_user);
void m_free(void *ptr);

#endif