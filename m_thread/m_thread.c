#include "m_thread.h"
#include <ucontext.h>
#include <sys/ucontext.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <bits/sigaction.h>
#include <bits/sigstack.h>

#define MS_TO_NS(ms) (ms * 1000000)

#define INTERRUPT_SIGNAL SIGUSR1

// 10ms
#define INTERRUPT_INTERVAL MS_TO_NS(10)

// magic number
#define SWAPCONTEXT_SIZE 1024
#define SETCONTEXT_SIZE 1024

typedef struct TaskStruct_t {
    m_thread_t thread_id;
    struct TaskStruct_t *next;

    char *stack;
    ucontext_t *context;
} TaskStruct_t;

// task_list
struct TaskList_t {
    TaskStruct_t sentinel;
} task_list;

// pointer to the current task
static volatile TaskStruct_t *current;

// used to indicate thread id
static m_thread_t thread_count;

// schedule_context: context of schedule() function
static ucontext_t schedule_context;

// return_context: uc_link of user thread, which runs after user thread return, handles task delete
static ucontext_t return_context;

// schedule context has started
static int started;

// global timer
static timer_t timer;

// push a task into task list
static void pushTask(TaskStruct_t *task) {
    if (!task) {
        return;
    }
    TaskStruct_t *curr = &task_list.sentinel;
    while (curr->next) {
        curr = curr->next;
    }
    curr->next = task;
}

// get next task to execute of given current
// return might be null
// basically round-robin
static TaskStruct_t *getNextTask(volatile TaskStruct_t *prev) {
    if (!prev) {
        return task_list.sentinel.next;
    }
    if (prev->next) {
        return prev->next;
    }
    return task_list.sentinel.next;
}

// remove task from task_list, on success, return removed task, on failure, return NULL
static TaskStruct_t *removeTask(volatile TaskStruct_t *task) {
    if (!task) {
        return NULL;
    }

    TaskStruct_t *prev = &task_list.sentinel, *curr = task_list.sentinel.next;
    while (curr && curr != task) {
        prev = curr;
        curr = curr->next;
    }
    if (curr) {
        prev->next = curr->next;
        curr->next = NULL;
        return curr;
    }

    fprintf(stderr, "[Bug: task not in task_list]\n");
    return NULL;
}

// free memory owned by task
static void freeTask(TaskStruct_t *task) {
    free(task->stack);
    free(task->context);
    free(task);
}

// allocate memory for a task
static TaskStruct_t *allocateTask() {

    TaskStruct_t *task = malloc(sizeof(TaskStruct_t));
    if (!task) {
        return NULL;
    }

    task->thread_id = -1;
    task->next = NULL;
    task->context = malloc(sizeof(ucontext_t));
    task->stack = malloc(SIGSTKSZ);

    if (!task->context || !task->stack) {
        freeTask(task);
        return NULL;
    }

    return task;
}

static void schedule() {
    while (1) {
        // current is not null: from timer interrupt
        if (current) {
            current = getNextTask(current);
            if (current) {
                // printf("[Enter thread %lu]\n", current->thread_id);
                swapcontext(&schedule_context, current->context);
                // back from thread context
            } else {
                fprintf(stderr, "[Bug: no next task]\n");
                return;
            }
        // current is null: from return_context or initial
        } else {
            current = getNextTask(current);
            if (current) {
                // printf("[Enter thread %lu]\n", current->thread_id);
                swapcontext(&schedule_context, current->context);
                // back from thread context
            } else {
                printf("[All tasks finish]\n");
                return;
            }
        }
    }
}

// handle thread return, this context blocks timer interrupt
static void handleReturn() {
    if (current) {
        if (removeTask(current)) {
            freeTask((TaskStruct_t *)current);
            current = NULL;
        } else {
            fprintf(stderr, "[Bug: current %p not in task_list]\n", current);
            current = NULL;
        }
    } else {
        fprintf(stderr, "[Bug: null current in handleReturn]\n");
    }

    setcontext(&schedule_context);
}

static int isManipulatingContext(void *ip) {
    void *base_swap = swapcontext;
    void *base_set = setcontext;
    return (ip >= base_swap && ip <= base_swap + SWAPCONTEXT_SIZE)
        || (ip >= base_set && ip <= base_set + SETCONTEXT_SIZE);
}

static void *getInterruptIP(void *ucontext) {
    ucontext_t *uc = ucontext;
    struct sigcontext *frame = (struct sigcontext *)&uc->uc_mcontext;
#ifdef __x86_64__
    return (void *)frame->rip;
#else
    return (void *)frame->eip;
#endif
}

static void timerInterrupt(int sig, siginfo_t *info, void *ucontext) {
    if (current) {

        // printf("[Timer interrupt %lu]\n", current->thread_id);

        void *ip = getInterruptIP(ucontext);
        if (isManipulatingContext(ip)) {
            // printf("[In critical section]\n");
            // thread is manipulating context, do nothing
            return;
        }

        // switch to scheduler context
        swapcontext(current->context, &schedule_context);
        // back from scheduler context, continue execution
    } else {
        fprintf(stderr, "[Bug: No current in timer interrupt]\n");
    }
}

static void installTimer() {
    struct sigevent sev = {.sigev_signo = INTERRUPT_SIGNAL, .sigev_notify = SIGEV_SIGNAL};
    if (timer_create(CLOCK_REALTIME, &sev, &timer)) {
        perror("timer create failed in installTimer");
    }
    struct itimerspec spec = {.it_interval.tv_nsec = INTERRUPT_INTERVAL, .it_value.tv_nsec = INTERRUPT_INTERVAL};
    if (timer_settime(timer, 0, &spec, NULL)) {
        perror("timer settime failed in installTimer");
    }

}

static void uninstallTimer() {
    struct itimerspec spec = {0};
    if (timer_settime(timer, 0, &spec, NULL)) {
        perror("timer settime failed in uninstallTimer");
    }
}

static void installInterruptHandler() {
    struct sigaction action = {0};
    action.sa_sigaction = timerInterrupt;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(INTERRUPT_SIGNAL, &action, NULL);
}

static void uninstallInterruptHandler() {
    struct sigaction action = {0};
    action.sa_handler = SIG_DFL;
    sigaction(INTERRUPT_SIGNAL, &action, NULL);
}

void blockInterrupt() {
    sigset_t set;
    sigprocmask(SIG_SETMASK, NULL, &set);
    sigaddset(&set, INTERRUPT_SIGNAL);
    sigprocmask(SIG_SETMASK, &set, NULL);
}

void unblockInterrupt() {
    sigset_t set;
    sigprocmask(SIG_SETMASK, NULL, &set);
    sigdelset(&set, INTERRUPT_SIGNAL);
    sigprocmask(SIG_SETMASK, &set, NULL);
}

m_thread_t m_thread_self() {
    blockInterrupt();
    if (current) {
        m_thread_t id = current->thread_id;
        unblockInterrupt();
        return id;
    }
    unblockInterrupt();
    return -1;
}

int m_thread_yield() {
    blockInterrupt();
    if (current) {
        // go back to scheduler
        swapcontext(current->context, &schedule_context);
        // return from scheduler
        unblockInterrupt();
        return 0;
    }

    // not in a thread
    unblockInterrupt();
    return -1;
}

void m_thread_sleep(unsigned int sec) {
    m_thread_usleep(sec * 1000000);
}

void m_thread_usleep(unsigned int us) {
    volatile clock_t s = clock();
    while (clock() - s <= us) {
        m_thread_yield();
    }
}

int m_thread_create(m_thread_t *ret, void (*func)(void *), void *arg) {
    if (!func || !ret) {
        return -1;
    }

    TaskStruct_t *task = allocateTask();
    if (!task) {
        return -1;
    }

    if (getcontext(task->context)) {
        freeTask(task);
        return -1;
    }

    task->context->uc_stack.ss_sp = task->stack;
    task->context->uc_stack.ss_size = SIGSTKSZ;
    task->context->uc_link = &return_context;

    // user thread: do not block timer interrupt
    sigdelset(&task->context->uc_sigmask, INTERRUPT_SIGNAL);

    typedef void (*func_ptr) (void);
    makecontext(task->context, (func_ptr)func, 1, arg);

    // enter critical section
    blockInterrupt();

    task->thread_id = thread_count++;
    pushTask(task);

    unblockInterrupt();

    *ret = task->thread_id;

    return 0;
}

int m_thread_start() {
    if (started) {
        return -1;
    }

    // scheduler needs block interrupt
    blockInterrupt();

    // setup function return context
    if (getcontext(&return_context)) {
        unblockInterrupt();
        return -1;
    }
    char stack[SIGSTKSZ];
    return_context.uc_stack.ss_sp = stack;
    return_context.uc_stack.ss_size = SIGSTKSZ;
    makecontext(&return_context, handleReturn, 0);

    installInterruptHandler();
    installTimer();

    // enter schedule context
    if (getcontext(&schedule_context)) {
        uninstallTimer();
        uninstallInterruptHandler();
        unblockInterrupt();
        return -1;
    }
    started = 1;
    schedule();
    started = 0;

    // exit clean up
    uninstallTimer();
    uninstallInterruptHandler();
    unblockInterrupt();

    return 0;
}