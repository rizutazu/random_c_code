#ifndef m_thread_h
#define m_thread_h

#include <stdint.h>

typedef int64_t m_thread_t;

// create && add a new thread, its thread id stored in ret
// the thread won't start automatically
int m_thread_create(m_thread_t *ret, void (*func)(void *), void *arg);

// give up the cpu in thread
int m_thread_yield();

// get current thread id, if it is not in the thread, return -1
m_thread_t m_thread_self();

// start all the threads created before, block until everything finish
int m_thread_start();

#endif