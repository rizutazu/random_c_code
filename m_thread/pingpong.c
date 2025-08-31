#include <stdio.h>
#include <unistd.h>

#include "m_thread.h"

#define PINGPONG_ROUND 3

struct fds {
    int read;
    int write;
};

void write_read(void *arg) {
    struct fds *fds = arg;
    char buf = 'a';
    int count = 0;
    while (count < PINGPONG_ROUND) {

        printf("Thread %lu: --> %c\n", m_thread_self(), buf);
        write(fds->write, &buf, 1);

        read(fds->read, &buf, 1);
        printf("Thread %lu: <-- %c\n", m_thread_self(), buf);

        printf("Thread %lu: %c++ => %c\n", m_thread_self(), buf, buf + 1);
        buf++;

        count++;
    }

}

void read_write(void *arg) {
    struct fds *fds = arg;
    char buf = 'a';
    int count = 0;
    while (count < PINGPONG_ROUND) {

        read(fds->read, &buf, 1);
        printf("Thread %lu: <-- %c\n", m_thread_self(), buf);

        printf("Thread %lu: %c++ => %c\n", m_thread_self(), buf, buf + 1);
        buf++;

        printf("Thread %lu: --> %c\n", m_thread_self(), buf);
        write(fds->write, &buf, 1);

        count++;
    }
}

int main() {
    int fd0[2], fd1[2];
    if (pipe(fd0) || pipe(fd1)) {
        printf("pipe failed\n");
        return -1;
    }
    struct fds fds0 = {.read = fd0[0], .write = fd1[1]};
    struct fds fds1 = {.read = fd1[0], .write = fd0[1]};

    m_thread_t t1, t2;
    m_thread_create(&t1, write_read, &fds0);
    m_thread_create(&t2, read_write, &fds1);
    m_thread_start();

    return 0;
}
