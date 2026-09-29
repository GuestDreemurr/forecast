#ifndef CHANNEL_FADE_H
#define CHANNEL_FADE_H
#include <types.h>
#include <revolution/GX.h>
#include <revolution/MTX.h>

// Full-screen gradient fade (d_scene's m_pFade / m_pFade2)
class Fade {
public:
    void Draw();

    GXColor mColors[4]; // at 0x0, one per corner
    u8 unk10[0x4];      // at 0x10
    Vec mQuad[4];       // at 0x14
};

#endif
