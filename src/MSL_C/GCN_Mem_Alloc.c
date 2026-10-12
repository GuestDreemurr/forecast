#include <revolution/OS/OSAlloc.h>
#include <revolution/OS/OSArena.h>
#include <revolution/OS/OSError.h>

#pragma exceptions off

#define ROUND_UP_32B(x) (((u32)(x) + 31) & ~31)
#define ROUND_DOWN_32B(x) ((u32)(x) & ~31)

// Turns the whole arena into the current heap when the game has not set one up
static void InitDefaultHeap(void) {
    void* arenaLo;
    void* arenaHi;

    OSReport("GCN_Mem_Alloc.c : InitDefaultHeap. No Heap Available\n");
    OSReport("Metrowerks CW runtime library initializing default heap\n");

    arenaLo = OSGetArenaLo();
    arenaHi = OSGetArenaHi();

    arenaLo = OSInitAlloc(arenaLo, arenaHi, 1);
    OSSetArenaLo(arenaLo);

    arenaLo = (void*)ROUND_UP_32B(arenaLo);
    arenaHi = (void*)ROUND_DOWN_32B(arenaHi);

    OSSetCurrentHeap(OSCreateHeap(arenaLo, arenaHi));
    OSSetArenaLo(arenaLo = arenaHi);
}

__declspec(weak) void __sys_free(void* ptr) {
    if (__OSCurrHeap == -1) {
        InitDefaultHeap();
    }
    OSFreeToHeap(__OSCurrHeap, ptr);
}
