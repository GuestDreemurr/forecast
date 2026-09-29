#ifndef CHANNEL_FADE_H
#define CHANNEL_FADE_H
#include <types.h>
#include <revolution/GX.h>
#include <revolution/MTX.h>
#include <channel/Color.h>
#include <channel/Vec3.h>
#include <nw4r/ut/ut_Color.h>

// Full-screen gradient fade (d_scene's m_pFade / m_pFade2)
class Fade {
public:
    typedef BOOL (Fade::*StateFunc)();

    Fade(nw4r::ut::Color color);
    ~Fade();

    void Draw();
    void Calc();
    void FadeIn(s32 frames);
    void FadeOut(s32 frames);
    void SetOpaque();

    BOOL StateIdle();
    BOOL StateFadeIn();
    BOOL StateFadeOut();

    void ChangeState(StateFunc state) {
        if (mState) {
            mStatePhase = -1;
            (this->*mState)();
        }

        mState = state;
        mStatePhase = 0;
        (this->*mState)();
    }

    Color mColors[4];     // at 0x0, one per corner
    GXColor mBaseColor;   // at 0x10
    Vec3 mQuad[4];        // at 0x14
    StateFunc mState;     // at 0x44
    s32 mFading;          // at 0x50, 1 = fading in, 2 = fading out
    s32 mStatePhase;      // at 0x54
    s32 mFrames;          // at 0x58
    f32 mAlpha;           // at 0x5C
    f32 mStep;            // at 0x60
};

#endif
