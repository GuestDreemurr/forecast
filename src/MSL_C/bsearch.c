#include "size_t.h"

#define NULL 0

void* bsearch(const void* key, const void* base, size_t num, size_t size, int (*compare)(const void*, const void*)) {
    const char* p;
    size_t l, u, m;
    int c;

    if (!key || !base || !num || !size || !compare) {
        return NULL;
    }

    if ((c = compare(key, base)) == 0) {
        return (void*)base;
    }

    if (c < 0) {
        return NULL;
    }

    l = 1;
    u = num - 1;

    while (l <= u) {
        m = (l + u) >> 1;
        p = (const char*)base + size * m;

        if ((c = compare(key, p)) == 0) {
            return (void*)p;
        }

        if (c < 0) {
            u = m - 1;
        }
        else {
            l = m + 1;
        }
    }

    return NULL;
}
