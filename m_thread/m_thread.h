#ifndef m_thread_h
#define m_thread_h

#include <stdint.h>

typedef int64_t m_thread_t;

// create && add a new thread, its thread id stored in ret
// the thread won't start automatically
// ret and func shall not be NULL
int m_thread_create(m_thread_t *ret, void (*func)(void *), void *arg);

// give up the cpu in thread
int m_thread_yield();

// get current thread id, if it is not in the thread, return -1
m_thread_t m_thread_self();

// sleep sec seconds
void m_thread_sleep(unsigned int sec);

// sleep us useconds
void m_thread_usleep(unsigned int us);

// start all the threads created before, block until everything finish
int m_thread_start();

// if you don't want any interruption to be happened during expression `x`, use this
// example: block_interruption_execute(x = x + 1;)
// trick: make all calls to a specific function block interruption:
    // #define your_function(...) block_interruption_execute(your_function(__VA_ARGS__))
    // e.g., printf:
    // #define printf(...) block_interruption_execute(printf(__VA_ARGS__))
#define block_interruption_execute(x) do {   \
    extern void blockInterrupt();   \
    extern void unblockInterrupt(); \
    blockInterrupt();   \
    x;  \
    unblockInterrupt(); \
} while (0)

#endif