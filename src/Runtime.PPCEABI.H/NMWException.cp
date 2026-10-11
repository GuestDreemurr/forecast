#include "Runtime.PPCEABI.H/NMWException.h"
#include <stddef.h>

typedef void* ConstructorDestructor;

#define DTORCALL_COMPLETE(dtor, objptr) (((void (*)(void*, short))dtor)(objptr, -1))

// Destroys the n elements of an array from last to first
extern "C" void __destroy_arr(void* block, ConstructorDestructor* dtor, size_t size, size_t n) {
    char* p;

    for (p = (char*)block + size * n; n > 0; n--) {
        p -= size;
        DTORCALL_COMPLETE(dtor, p);
    }
}
