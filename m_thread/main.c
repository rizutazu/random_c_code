#include <stdio.h>
#include <unistd.h>
#include "m_thread.h"

void func1(void *arg) {
    int i = 0;
    while (1) {
        printf("[thr %lu]func1! %s, count %d\n", m_thread_self(), (char *)arg, i++);
        usleep(10000);
        if (i % 5 == 0) {
            m_thread_yield();
        }
        if (i > 50) {
            printf("func1 bye!\n");
            return;
        }
    }
}

void func2(void *arg) {
    int i = 0;
    while (1) {
        printf("[thr %lu]func22! %s, count %d\n", m_thread_self(), (char *)arg, i++);
        usleep(10000);
        if (i % 10 == 0) {
            m_thread_yield();
        }
        if (i > 100) {
            printf("func22! bye!\n");
            printf("func22! add tasks!\n");
            m_thread_t t;
            m_thread_create(&t, func1, arg);
            return;
        }
    }
}

void func3(void *arg) {
    int i = 0;
    while (1) {
        printf("[thr %lu]func333! %s, count %d\n", m_thread_self(), (char *)arg, i++);
        usleep(10000);
        if (i % 20 == 0) {
            m_thread_yield();
        }
        if (i > 200) {
            printf("func333! bye!\n");
            printf("func333! add tasks!\n");
            m_thread_t t;
            m_thread_create(&t, func2, arg);
            m_thread_create(&t, func2, arg);
            return;
        }
    }
}

int main() {
    m_thread_t t1, t2, t3;
    char *arg1 = "1";
    char *arg2 = "2";
    char *arg3 = "3";
    m_thread_create(&t1, func1, arg1);
    m_thread_create(&t2, func2, arg2);
    m_thread_create(&t3, func3, arg3);
    m_thread_start();
    return 0;
}
