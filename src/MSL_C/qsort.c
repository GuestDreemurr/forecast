#include <cstdlib>

// Swaps two elements of the given width byte by byte
#define swap_elements(a, b, width)   \
    do {                             \
        char* src = (b) - 1;         \
        char* dst = (a) - 1;         \
        size_t left = (width) + 1;   \
        long tmp;                    \
                                     \
        while (--left) {             \
            tmp = *++dst;            \
            *dst = *++src;           \
            *src = tmp;              \
        }                            \
    } while (0)

// Heapsort. Element positions are 1-based: element i lives at base + (i - 1) * width
void qsort(void* base, size_t count, size_t width, int (*compare)(const void*, const void*)) {
    size_t start;
    size_t end;
    size_t child;
    char* startElem;
    char* endElem;
    char* parentElem;
    char* childElem;
    char* siblingElem;

    if (count < 2) {
        return;
    }

    end = count;
    start = count / 2 + 1;
    startElem = (char*)base + width * (start - 1);
    endElem = (char*)base + width * (end - 1);

    for (;;) {
        // First build the heap, then repeatedly move the root behind the heap
        if (start > 1) {
            start--;
            startElem -= width;
        }
        else {
            swap_elements(startElem, endElem, width);
            if (--end == 1) {
                return;
            }
            endElem -= width;
        }

        // Sift the element at start down
        child = start;
        childElem = (char*)base + width * (child - 1);

        while (child * 2 <= end) {
            child *= 2;
            parentElem = childElem;
            childElem = (char*)base + width * (child - 1);

            if (child < end) {
                siblingElem = childElem + width;
                if (compare(childElem, siblingElem) < 0) {
                    child++;
                    childElem = siblingElem;
                }
            }

            if (compare(parentElem, childElem) >= 0) {
                break;
            }
            swap_elements(parentElem, childElem, width);
        }
    }
}
