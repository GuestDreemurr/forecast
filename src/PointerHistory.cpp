#include <channel/PointerHistory.h>
#include <channel/System.h>

PointerHistory::PointerHistory() {
    mIndex = 0;
}

PointerHistory::Entry::Entry() {
    x = 0.0f;
    y = 0.0f;
    valid = FALSE;
}

PointerHistory::Entry::~Entry() {}

void PointerHistory::Reset() {
    mIndex = 0;

    for (int i = 0; i < POINTER_HISTORY_CHANNELS; i++) {
        for (int j = 0; j < POINTER_HISTORY_SIZE; j++) {
            mEntries[i][j].x = 0.0f;
            mEntries[i][j].y = 0.0f;
            mEntries[i][j].valid = FALSE;
        }
    }
}

void PointerHistory::Update() {
    for (int i = 0; i < POINTER_HISTORY_CHANNELS; i++) {
        mEntries[i][mIndex].x = gPointerX[i][0];
        mEntries[i][mIndex].y = gPointerY[i][0];

        BOOL valid = FALSE;
        if (gPointerValid[i][0] && gKPADLatest[i] >= 0) {
            valid = TRUE;
        }
        mEntries[i][mIndex].valid = valid;
    }

    if (++mIndex >= POINTER_HISTORY_SIZE) {
        mIndex = 0;
    }
}

BOOL PointerHistory::GetOldest(s32 chan, f32* pX, f32* pY) {
    s32 index = mIndex;

    for (int i = 0; i < POINTER_HISTORY_SIZE; i++) {
        Entry* entry = &mEntries[chan][index];
        if (entry->valid) {
            *pX = entry->x;
            *pY = entry->y;
            return TRUE;
        }

        if (++index >= POINTER_HISTORY_SIZE) {
            index = 0;
        }
    }

    return FALSE;
}
