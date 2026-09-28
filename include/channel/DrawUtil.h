#ifndef CHANNEL_DRAW_UTIL_H
#define CHANNEL_DRAW_UTIL_H
#include <types.h>

#include <revolution/GX.h>
#include <revolution/MTX.h>
#include <revolution/TPL.h>

struct Rect {
    f32 left;   // at 0x0
    f32 top;    // at 0x4
    f32 right;  // at 0x8
    f32 bottom; // at 0xC
};

enum TextureFlip {
    TEXTURE_FLIP_S = (1 << 0),
    TEXTURE_FLIP_T = (1 << 1),
};

void DrawTexture(TPLPalette* palette, u32 id, f32 scaleX, f32 scaleY, const Vec* pos, u32 flip);
void DrawQuad(const Vec* verts, const GXColor* color);
void DrawRect(const Rect* rect, const GXColor* color);
void DrawGradientQuad(const Vec* verts, const GXColor* colors);
void DrawGradientRect(const Rect* rect, const GXColor* colors);
void DrawRectOutline(const Rect* rect, GXColor color, u8 width);

#endif
