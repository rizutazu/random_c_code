#include <stdio.h>
#include "c_try_catch.h"

#define Exception0 0
#define Exception1 1
#define Exception2 2

int recursive(int level) {
    try(0, {
        if (level == 2) {
            throw(Exception0, NULL);
        }
        if (level == 5) {
            throw(Exception1, NULL);
        }
        if (level == 8) {
            NULL;
            throw(Exception2, NULL);
        }
        printf("level %d\n", level);
        recursive(level + 1);
    }) catch(0, Exception0, data, {
        printf("catch 0 at level %d\n", level);
        recursive(level + 1);
    }) catch(0, Exception1, data, {
        printf("catch 1 at level %d\n", level);
        recursive(level + 1);
    }) finally(0)
    return 0;
}

int main() {
    try(0, {
        recursive(0);
    }) catch(0, Exception2, data, {
        printf("catch 2 at main\n");
    }) finally(0)
}
