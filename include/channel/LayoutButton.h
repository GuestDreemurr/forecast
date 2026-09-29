#ifndef CHANNEL_LAYOUT_BUTTON_H
#define CHANNEL_LAYOUT_BUTTON_H
#include <types.h>
#include <cstring>

// The nw4r::lyt::Pane a button belongs to (this nw4r version keeps the name at 0xB4)
struct ButtonPane {
    u8 unk0[0xB4];  // at 0x0
    char mName[17]; // at 0xB4
};

// A clickable layout pane
class LayoutButton {
public:
    void Hover();
    BOOL Release();
    void Press(u8 arg);

    BOOL IsName(const char* name) const {
        return std::strcmp(mPane->mName, name) == 0;
    }

    BOOL IsInactive() const {
        return mDisabled || mLocked || mHidden;
    }

    u8 unk0[0x8];          // at 0x0
    ButtonPane* mPane;     // at 0x8
    u8 unkC[0x90 - 0xC];   // at 0xC
    u8 mDisabled;          // at 0x90
    u8 mHeld;              // at 0x91
    u8 unk92[0x94 - 0x92]; // at 0x92
    u8 mHidden;            // at 0x94
    u8 unk95[0x98 - 0x95]; // at 0x95
    u8 mLocked;            // at 0x98
};

// A set of buttons hit-tested together
class ButtonGroup {
public:
    LayoutButton* HitTest(f32 x, f32 y);
};

#endif
