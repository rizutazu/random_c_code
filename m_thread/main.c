#include <stdio.h>
#include <unistd.h>
#include "m_thread.h"

void func1(void *arg) {
    int i = 0;
    while (1) {
        printf("[Thread %lld]func1! %s, count %d\n", m_thread_self(), (char *)arg, i++);
        m_thread_usleep(50000);
        if (i % 5 == 0) {
            m_thread_yield();
        }
        if (i > 50) {
            printf("[Thread %llu]func1 bye!\n", m_thread_self());
            return;
        }
    }
}

void func2(void *arg) {
    int i = 0;
    while (1) {
        printf("[Thread %lld]func22! %s, count %d\n", m_thread_self(), (char *)arg, i++);
        m_thread_usleep(25000);
        if (i % 10 == 0) {
            m_thread_yield();
        }
        if (i > 100) {
            printf("[Thread %lld]func22! bye!\n", m_thread_self());
            m_thread_t t;
            m_thread_create(&t, func1, arg);
            printf("[Thread %lld]func22! add tasks!: %lld\n", m_thread_self(), t);
            return;
        }
    }
}

void func3(void *arg) {
    int i = 0;
    while (1) {
        printf("[Thread %lld]func333! %s, count %d\n", m_thread_self(), (char *)arg, i++);
        m_thread_usleep(15000);
        if (i % 20 == 0) {
            m_thread_yield();
        }
        if (i > 200) {
            printf("[Thread %lld]func333! bye!\n", m_thread_self());
            m_thread_t t;
            m_thread_create(&t, func2, arg);
            printf("[Thread %lld]func333! add tasks!: %llu\n", m_thread_self(), t);
            m_thread_create(&t, func2, arg);
            printf("[Thread %lld]func333! add tasks!: %llu\n", m_thread_self(), t);
            return;
        }
    }
}

void round1() {
    m_thread_t t1, t2;
    char *arg1 = "rrr";
    char *arg2 = "bbb";
    m_thread_create(&t1, func1, arg1);
    m_thread_create(&t2, func2, arg2);
    m_thread_start();
}

void round2() {
    m_thread_t t1, t2, t3;
    char *arg1 = "1";
    char *arg2 = "2";
    char *arg3 = "3";
    m_thread_create(&t1, func1, arg1);
    m_thread_create(&t2, func2, arg2);
    m_thread_create(&t3, func3, arg3);
    m_thread_start();
}

int main() {
    round1();
    printf("round1 ok, wait 2 sec...\n");
    sleep(2);
    round2();
    printf("round2 ok\n");
    return 0;
}
