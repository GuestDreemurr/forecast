#ifndef CHANNEL_FADE_H
#define CHANNEL_FADE_H
#include <types.h>
#include <revolution/GX.h>
#include <revolution/MTX.h>

// Full-screen gradient fade (d_scene's m_pFade / m_pFade2)
class Fade {
public:
    void Draw();
    void Calc();
    void FadeIn(s32 frames);
    void FadeOut(s32 frames);

    GXColor mColors[4]; // at 0x0, one per corner
    u8 unk10[0x4];      // at 0x10
    Vec mQuad[4];       // at 0x14
    u8 unk44[0x50 - 0x44]; // at 0x44, state function
    s32 mFading;        // at 0x50, 1 = fading in, 2 = fading out
};

#endif
