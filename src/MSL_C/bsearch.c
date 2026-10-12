#include <stddef.h>
#include <cstdlib>

// Binary search over count sorted elements of the given width. Checks the first element on its
// own, then searches the rest (indices 1 to count - 1)
void* bsearch(const void* key, const void* base, size_t count, size_t width, int (*compare)(const void*, const void*)) {
    const char* elem;
    size_t low;
    size_t high;
    size_t mid;
    int order;

    if (key == NULL || base == NULL || count == 0 || width == 0 || compare == NULL) {
        return NULL;
    }

    order = compare(key, base);
    if (order == 0) {
        return (void*)base;
    }
    if (order < 0) {
        return NULL;
    }

    low = 1;
    high = count - 1;

    while (low <= high) {
        mid = (low + high) >> 1;
        elem = (const char*)base + width * mid;

        order = compare(key, elem);
        if (order == 0) {
            return (void*)elem;
        }

        if (order < 0) {
            high = mid - 1;
        }
        else {
            low = mid + 1;
        }
    }

    return NULL;
}
