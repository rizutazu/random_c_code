#include "c_try_catch.h"
#include <stdio.h>

void random_free(char *arg) {
    printf("free? %s\n", arg);
}

void directly_exit(char *arg) {
    printf("i will exit directly %s\n", arg);
    exit(42);
}

void func3() {
    register_clean_func(random_free, "func3");
    throw(114, NULL);
}

void func2() {
    register_clean_func(directly_exit, "func2");
    func3();
}

void func1() {
    register_clean_func(random_free, "func1");
    func2();
}

int main() {
    register_clean_func(random_free, "main");
    func1();
    printf("..\n");
}
