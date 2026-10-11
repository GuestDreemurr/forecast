#include "Runtime.PPCEABI.H/NMWException.h"

#define MAXFRAGMENTS 1

typedef struct ProcessInfo {
    struct __eti_init_info* exception_info;
    char* TOC;
    int active;
} ProcessInfo;

static ProcessInfo fragmentinfo[MAXFRAGMENTS];

// Records a code fragment's exception tables, returns its ID or -1 when the table is full
int __register_fragment(struct __eti_init_info* info, char* TOC) {
    ProcessInfo* f;
    int i;

    for (i = 0, f = fragmentinfo; i < MAXFRAGMENTS; ++i, ++f) {
        if (f->active == 0) {
            f->exception_info = info;
            f->TOC = TOC;
            f->active = 1;
            return i;
        }
    }
    return -1;
}

void __unregister_fragment(int fragmentID) {
    ProcessInfo* f;

    if (fragmentID >= 0 && fragmentID < MAXFRAGMENTS) {
        f = &fragmentinfo[fragmentID];
        f->exception_info = 0;
        f->TOC = 0;
        f->active = 0;
    }
}
