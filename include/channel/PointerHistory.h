#ifndef CHANNEL_POINTER_HISTORY_H
#define CHANNEL_POINTER_HISTORY_H
#include <types.h>

#define POINTER_HISTORY_CHANNELS 4
#define POINTER_HISTORY_SIZE 10

// Ring buffer of the last few pointer positions of each controller
class PointerHistory {
public:
    struct Entry {
        Entry();
        ~Entry();

        f32 x;    // at 0x0
        f32 y;    // at 0x4
        u8 valid; // at 0x8
    };

    PointerHistory();

    void Reset();
    void Update();
    BOOL GetOldest(s32 chan, f32* pX, f32* pY);

    Entry mEntries[POINTER_HISTORY_CHANNELS][POINTER_HISTORY_SIZE]; // at 0x0
    s32 mIndex;                                                     // at 0x1E0
};

#endif
