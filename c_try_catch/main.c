#include <stdio.h>
#include "c_try_catch.h"

#define BadShitHappenedException0 0
#define WhatHellException1 1
#define IDKException2 2

void free3(void *ptr) {
    printf("clean main!\n");
    free(ptr);
}

void free2(void *ptr) {
    printf("clean 2!\n");
    free(ptr);
}

void free1(void *ptr) {
    printf("clean 1!\n");
    free(ptr);
}

void func3() {
}

void func2() {
    void *ptr = malloc(12);
    void *token = register_clean_func(free2, ptr);
    printf("func 2 register clean %p\n", token);
    func3();
    unregister_clean_func(token);
    printf("func 2 unregister clean %p\n", token);
    free(ptr);

    throw(BadShitHappenedException0, NULL);
}

void func1() {
    printf("func1!\n");
    void *ptr = malloc(12);
    void *token = register_clean_func(free1, ptr);
    printf("func 1 register clean %p\n", token);
    func2();
    printf("func 1 unregister clean %p\n", token);
    unregister_clean_func(token);

    try(0, {
        throw(WhatHellException1, "llllll");
    }) catch(0, WhatHellException1, data, {
        printf("what hell %s\n", (char *)data);
        throw(BadShitHappenedException0, "?");
    }) finally(0)

    printf("func1 return\n");
}

int main() {
    int a = 114514;

    try(0, {
        void *ptr = malloc(114);
        printf("hi\n");
        void *token = register_clean_func(free3, ptr);
        func1();
        unregister_clean_func(token);
    }) catch(0, BadShitHappenedException0, data, {
        printf("booo %s\n", (char *)data);
    }) catch(0, BadShitHappenedException0, data, {
        printf("booo ver2 %s\n", (char *)data);
    }) finally(0)

    // try(3, {
    //     try(2, {
    //         try(1, {
    //             throw(BadShitHappenedException0, "boommmmm");
    //         }) catch(1, BadShitHappenedException0, data, {
    //             printf("bad shit happened 0 %s\n", (char *)data);
    //             printf("my lovely local %d\n", a);
    //             throw(WhatHellException1, data + 1);
    //         })
    //         // catch(1, IDKException2, data, {
    //         //     printf("what?\n");
    //         // })
    //         finally(1)
    //     }) catch(2, WhatHellException1, data,{
    //         printf("what hell 1 %s\n", (char *)data);
    //         printf("my lovely local %d\n", a);
    //         throw(IDKException2, data + 1);
    //     }) finally(2)
    // }) catch(3, IDKException2, data,{
    //     printf("i don't know 2 %s\n", (char *)data);
    //     printf("my lovely local %d\n", a);
    // }) finally(3)

    printf("main ret\n");

    return 0;
}

void ww() {
}