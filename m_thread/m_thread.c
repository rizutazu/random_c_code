#include "m_thread.h"
#include <ucontext.h>
#include <sys/ucontext.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <bits/sigaction.h>
#include <sys/time.h>
#include <bits/sigstack.h>

#define TIMER_INTERRUPT_SIGNAL SIGALRM

// 10ms
#define INTERRUPT_INTERVAL 10000

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

    printf("Bug: task not in task_list\n");
    return NULL;
}

// free memory owned by task
static void freeTask(volatile TaskStruct_t *task) {
    free(task->stack);
    free(task->context);
    free((TaskStruct_t *)task);
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
                printf("[Bug: no next task]\n");
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
            freeTask(current);
            current = NULL;
        } else {
            printf("[Bug: current %p not in task_list]\n", current);
            current = NULL;
        }
    } else {
        printf("[Bug: null current in handleReturn]\n");
    }

    setcontext(&schedule_context);
}

static void timerInterrupt(int sig, siginfo_t *info, void *ucontext) {
    if (current) {
        // switch to scheduler context
        // printf("[Timer interrupt %lu]\n", current->thread_id);
        swapcontext(current->context, &schedule_context);
        // back from scheduler context, continue execution
    } else {
        //
        printf("[No current in timer interrupt]\n");
    }
}

static void installTimer() {
    // 10ms
    struct itimerval timer = {.it_interval.tv_usec = INTERRUPT_INTERVAL, .it_value.tv_usec = INTERRUPT_INTERVAL};
    setitimer(ITIMER_REAL, &timer, NULL);
}

static void uninstallTimer() {
    setitimer(ITIMER_REAL, NULL, NULL);
}

static void installInterruptHandler() {
    struct sigaction action = {0};
    action.sa_sigaction = timerInterrupt;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(TIMER_INTERRUPT_SIGNAL, &action, NULL);
}

static void uninstallInterruptHandler() {
    sigaction(TIMER_INTERRUPT_SIGNAL, NULL, NULL);
}

static void blockInterrupt() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, TIMER_INTERRUPT_SIGNAL);
    sigprocmask(SIG_BLOCK, &set, NULL);
}

static void unblockInterrupt() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, TIMER_INTERRUPT_SIGNAL);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
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
    sigdelset(&task->context->uc_sigmask, SIGALRM);

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