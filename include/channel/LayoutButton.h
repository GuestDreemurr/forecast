#ifndef CHANNEL_LAYOUT_BUTTON_H
#define CHANNEL_LAYOUT_BUTTON_H
#include <types.h>
#include <cstring>
#include <revolution/GX.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Color.h>
#include <channel/Vec2F.h>

namespace nw4r {
namespace lyt {
class Pane;
class DrawInfo;
class Layout;
class ArcResourceAccessor;
}
namespace math {
struct MTX34;
}
}

class TextTagProcessor;

// The nw4r::lyt::Pane a button belongs to (this nw4r version keeps the name at 0xB4)
struct ButtonPane {
    u8 unk0[0x2C];            // at 0x0
    f32 mTransX;              // at 0x2C
    f32 mTransY;              // at 0x30
    f32 mTransZ;              // at 0x34
    u8 unk38[0xB4 - 0x38];    // at 0x38
    char mName[17];           // at 0xB4
    u8 unkC5[0xCF - 0xC5];    // at 0xC5
    u8 mFlag;                 // at 0xCF, bit 2: scale the position on widescreen
};

// A clickable layout pane
class LayoutButton {
public:
    LayoutButton(nw4r::lyt::Pane* pane, nw4r::lyt::DrawInfo* drawInfo, void* soundInfo, TextTagProcessor* tagProcessor);
    ~LayoutButton();

    void Reset();
    void Calc();
    void Update();
    void Draw();
    BOOL Contains(f32 x, f32 y);
    void SetPaneAlpha(s32 alpha);
    void SetState(s32 state);
    void SetText(const wchar_t* text);
    Vec2F GetCenter();
    void SetPaneColor(const char* name, const nw4r::ut::Color& color, BOOL recursive);
    void Hide();
    void SetChildVisible(const char* name, u8 visible);
    void SetChildAlpha(const char* name, u8 alpha);
    void SetChildAlphaRecursive(nw4r::lyt::Pane* pane, u8 alpha);
    nw4r::lyt::Pane* FindPane(const char* name);
    void ShowLanguagePane(const char* prefix);

    void SetParams(s32 a, s32 b, s32 c) {
        unk74 = a;
        unk88 = b;
        unk8C = c;
    }

    void SetSlideOffset(f32 offset) {
        mSlideOffset = offset * (mPane->mTransY > 0.0f ? 1 : -1);
    }

    void SetLinked(LayoutButton* button) {
        mLinked = button;
        unk28 = 0;
    }

    void SetAlpha(s32 alpha) {
        mAlpha = alpha;
    }

    void Hover();
    BOOL Release();
    void Press(u8 arg);

    BOOL IsName(const char* name) const {
        return std::strcmp(mPane->mName, name) == 0;
    }

    void SetCalcCallback(void (*callback)(void* arg), void* arg) {
        mCalcCallback = callback;
        mCallbackArg = arg;
    }

    // Locked buttons ignore the pointer
    void Lock() {
        mLocked = TRUE;
        Release();
    }

    void Unlock() {
        mLocked = FALSE;
    }

    BOOL IsInactive() const {
        return mDisabled || mLocked || mHidden;
    }

    u8 unk0[0x8];                  // at 0x0
    ButtonPane* mPane;             // at 0x8
    u8 unkC[0x24 - 0xC];           // at 0xC
    LayoutButton* mLinked;         // at 0x24, the "set" pane shown next to this button
    s32 unk28;                     // at 0x28
    void* mCallbackArg;            // at 0x2C
    void (*mColorCallback)(nw4r::lyt::Pane* pane, const GXColor* color); // at 0x30
    void (*mCalcCallback)(void* arg); // at 0x34
    GXColor mTextColor;            // at 0x38
    f32 mLeft;                     // at 0x3C
    f32 mTop;                      // at 0x40
    f32 mRight;                    // at 0x44
    f32 mBottom;                   // at 0x48
    nw4r::math::VEC3 mOrigTrans;   // at 0x4C
    Vec mTextPos;                  // at 0x58
    Vec2 mTextSize;                // at 0x64
    u8 unk6C[0x70 - 0x6C];         // at 0x6C
    s32 mAlpha;                    // at 0x70
    s32 unk74;                     // at 0x74
    s32 mHoverFrame;               // at 0x78
    u8 unk7C[0x88 - 0x7C];         // at 0x7C
    s32 unk88;                     // at 0x88
    s32 unk8C;                     // at 0x8C
    u8 mDisabled;                  // at 0x90
    u8 mHeld;                      // at 0x91
    u8 mPressed;                   // at 0x92
    u8 mToggle;                    // at 0x93
    u8 mHidden;                    // at 0x94
    u8 unk95[0x98 - 0x95];         // at 0x95
    u8 mLocked;                    // at 0x98
    u8 unk99[0x9C - 0x99];         // at 0x99
    f32 mSlideOffset;              // at 0x9C
};

// Material TEV colors 0 and 1 of the pane's own material
void GetTevColors(nw4r::lyt::Pane* pane, GXColor* color0, GXColor* color1);
void SetTevColors(nw4r::lyt::Pane* pane, const GXColor* color0, const GXColor* color1);
s16 GetTevColor1Alpha(nw4r::lyt::Pane* pane);

// Linear interpolation from a to b as t goes from 0 to n
s32 Lerp(s32 a, s32 b, s32 t, s32 n);

#define BUTTON_GROUP_MAX_BUTTONS 256

// A layout whose top-level panes are all buttons, with slide and fade animations
class ButtonGroup {
public:
    ButtonGroup(void* arc, const char* layoutName, void* soundInfo, bool influencedAlpha);
    ~ButtonGroup();

    void Reset();
    void ReleaseAll();
    void Calc();
    void Draw();
    LayoutButton* HitTest(f32 x, f32 y);
    LayoutButton* FindButton(const char* name);
    void SlideOut(s32 frames);
    void SlideIn(s32 frames);
    void FadeIn(s32 frames);
    void SetButtonParams(s32 a, s32 b, s32 c);
    void SetPaneAlpha(s32 alpha);
    void SetViewMtx(const nw4r::math::MTX34& mtx);
    void SetSlideOffset(f32 offset);

    nw4r::lyt::ArcResourceAccessor* mResAccessor;        // at 0x0
    nw4r::lyt::Layout* mLayout;                          // at 0x4
    nw4r::lyt::DrawInfo* mDrawInfo;                      // at 0x8
    u8 unkC[0x10 - 0xC];                                 // at 0xC
    TextTagProcessor* mTagProcessor;                     // at 0x10
    LayoutButton* mButtons[BUTTON_GROUP_MAX_BUTTONS];    // at 0x14
    s32 mNumButtons;                                     // at 0x414
    u8 mSlidingOut;                                      // at 0x418
    s32 mSlideFrames;                                    // at 0x41C
    s32 mSlideFrame;                                     // at 0x420
    u8 mFadingOut;                                       // at 0x424
    s32 mFadeFrames;                                     // at 0x428
    s32 mFadeFrame;                                      // at 0x42C
    s32 mAlpha;                                          // at 0x430
};

#endif
