#include "m_malloc.h"
#include <stdint.h>

int main() {
    uint64_t *p = m_malloc(114514);
    *p = 32;
    m_free(p);

    return 0;
}