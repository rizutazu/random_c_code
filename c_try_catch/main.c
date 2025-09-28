#include <stdio.h>
#include "c_try_catch.h"

#define BadShitHappenedException0 0
#define WhatHellException1 1
#define IDKException2 2

void func1() {
    printf("func1!\n");
    try(0, {
        throw(WhatHellException1, "llllll");
    })
    catch(0, WhatHellException1, data, {
        printf("what hell %s\n", (char *)data);

        throw(BadShitHappenedException0, "?");
    }) finally(0)

    printf("func1 return\n");
}

int main() {
    int a = 114514;

    try(0, {
        printf("hi\n");
        func1();
    }) catch(0, BadShitHappenedException0, data, {
        printf("booo %s\n", (char *)data);
    }) catch(0, BadShitHappenedException0, data, {
        printf("booo ver2 %s\n", (char *)data);
    }) finally(0)

    try(3, {
        try(2, {
            try(1, {
                throw(BadShitHappenedException0, "boommmmm");
            }) catch(1, BadShitHappenedException0, data, {
                printf("bad shit happened 0 %s\n", (char *)data);
                printf("my lovely local %d\n", a);
                throw(WhatHellException1, data + 1);
            }) catch(1, IDKException2, data, {
                printf("what?\n");
            }) finally(1)
        }) catch(2, WhatHellException1, data,{
            printf("what hell 1 %s\n", (char *)data);
            printf("my lovely local %d\n", a);
            throw(IDKException2, data + 1);
        }) finally(2)
    }) catch(3, IDKException2, data,{
        printf("i don't know 2 %s\n", (char *)data);
        printf("my lovely local %d\n", a);
    }) finally(3)

    printf("main ret\n");

    return 0;
}