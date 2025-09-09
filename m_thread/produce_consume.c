#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "m_thread.h"

#define printf(...) async_signal_safe(printf(__VA_ARGS__))
#define free(...) async_signal_safe(free(__VA_ARGS__))

#define N_PRODUCER 2
#define N_CONSUMER 20

struct WriteFds {
    size_t n;
    int *fds;
};

void producer(void *arg) {
    struct WriteFds *fds = arg;
    while (1) {
        size_t idx = (size_t)random() % fds->n;
        long int random_product = random();
        printf("[P] give idx %llu with %ld\n", idx, random_product);
        write(fds->fds[idx], &random_product, sizeof(random_product));
        m_thread_sleep(random() % 5 + 2);
    }
}

void consumer(void *arg) {
    int fd = *(int *)arg;
    free(arg);
    while (1) {
        long int random_product;
        read(fd, &random_product, sizeof(random_product));
        printf("[Thread %2lld] got stuff %ld\n", m_thread_self(), random_product);
    }
}

int createConsumer() {
    int fds[2];
    if (pipe(fds)) {
        return -1;
    }
    int *r = malloc(sizeof(int));
    if (!r) {
        return -1;
    }
    *r = fds[0];
    m_thread_t t;
    m_thread_create(&t, consumer, r);
    return fds[1];
}

void createProducer(struct WriteFds *fds) {
    m_thread_t t;
    m_thread_create(&t, producer, fds);
}

int main() {
    int buf[N_CONSUMER];
    struct WriteFds fds;
    fds.n = N_CONSUMER;
    fds.fds = buf;

    for (int i = 0; i < N_CONSUMER; i++) {
        int feedFd;
        if ((feedFd = createConsumer()) == -1) {
            return -1;
        }
        fds.fds[i] = feedFd;
    }
    for (int i = 0; i < N_PRODUCER; i++) {
        createProducer(&fds);
    }

    m_thread_start();

    return 0;
}
