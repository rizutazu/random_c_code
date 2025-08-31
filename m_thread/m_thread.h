#ifndef m_thread_h
#define m_thread_h

#include <stdint.h>

typedef int64_t m_thread_t;

int m_thread_create(m_thread_t *ret, void (*func)(void *), void *arg);

int m_thread_start();

#endif