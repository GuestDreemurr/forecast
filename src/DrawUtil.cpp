#include <channel/DrawUtil.h>
#include <channel/Color.h>
#include <channel/System.h>

void DrawTexture(TPLPalette* palette, u32 id, f32 scaleX, f32 scaleY, const Vec* pos, u32 flip) {
    GXTexObj texObj;
    f32 x0, y0, x1, y1;
    f32 s0, s1, t0, t1;

    GetTexObj(palette, id, &texObj);
    GXLoadTexObj(&texObj, GX_TEXMAP0);

    x0 = pos->x;
    y0 = pos->y;
    x1 = pos->x + scaleX * GetTexWidth(palette, id);
    y1 = pos->y + scaleY * GetTexHeight(palette, id);

    if (flip & TEXTURE_FLIP_S) {
        s0 = 1.0f;
        s1 = 0.0f;
    } else {
        s0 = 0.0f;
        s1 = 1.0f;
    }

    if (flip & TEXTURE_FLIP_T) {
        t0 = 1.0f;
        t1 = 0.0f;
    } else {
        t0 = 0.0f;
        t1 = 1.0f;
    }

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x0, y0, pos->z);
    GXTexCoord2f32(s0, t0);
    GXPosition3f32(x1, y0, pos->z);
    GXTexCoord2f32(s1, t0);
    GXPosition3f32(x1, y1, pos->z);
    GXTexCoord2f32(s1, t1);
    GXPosition3f32(x0, y1, pos->z);
    GXTexCoord2f32(s0, t1);
    GXEnd();
}

static inline void SetupColorQuad(void) {
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE,
                  GX_AF_NONE);
    GXSetNumChans(1);
    GXSetNumTexGens(0);

    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetNumTevStages(1);
    GXSetNumIndStages(0);
}

void DrawQuad(const Vec* verts, const GXColor* color) {
    SetupColorQuad();

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    for (int i = 0; i < 4; i++) {
        GXPosition3f32(verts[i].x, verts[i].y, verts[i].z);
        GXColor1u32(*(const u32*)color);
    }
    GXEnd();
}

void DrawRect(const Rect* rect, const GXColor* color) {
    Vec verts[4];
    f32 left, top, bottom, right;

    top = rect->top;
    left = rect->left;
    bottom = rect->bottom;
    right = rect->right;

    verts[0].x = left;
    verts[0].y = top;
    verts[0].z = 0.0f;
    verts[1].x = left;
    verts[1].y = bottom;
    verts[1].z = 0.0f;
    verts[2].x = right;
    verts[2].y = bottom;
    verts[2].z = 0.0f;
    verts[3].x = right;
    verts[3].y = top;
    verts[3].z = 0.0f;

    DrawQuad(verts, color);
}

void DrawGradientQuad(const Vec* verts, const GXColor* colors) {
    SetupColorQuad();

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    for (int i = 0; i < 4; i++) {
        GXPosition3f32(verts[i].x, verts[i].y, verts[i].z);
        GXColor1u32(*(const u32*)&colors[i]);
    }
    GXEnd();
}

void DrawGradientRect(const Rect* rect, const GXColor* colors) {
    Vec verts[4];
    f32 left, top, bottom, right;

    top = rect->top;
    left = rect->left;
    bottom = rect->bottom;
    right = rect->right;

    verts[0].x = left;
    verts[0].y = top;
    verts[0].z = 0.0f;
    verts[1].x = left;
    verts[1].y = bottom;
    verts[1].z = 0.0f;
    verts[2].x = right;
    verts[2].y = bottom;
    verts[2].z = 0.0f;
    verts[3].x = right;
    verts[3].y = top;
    verts[3].z = 0.0f;

    DrawGradientQuad(verts, colors);
}

// Colors are taken end-first: needed for the by-value copies to land in the original's stack slots
static inline void DrawColorLine(const Vec* start, const Vec* end, u8 width,
                                 GXColor endColor, GXColor startColor) {
    DrawLine(start, end, width, startColor, endColor);
}

static inline void DrawSolidLine(const Vec* start, const Vec* end, u8 width,
                                 const GXColor& color) {
    DrawColorLine(start, end, width, color, color);
}

void DrawRectOutline(const Rect* rect, GXColor color, u8 width) {
    Vec topLeft, bottomLeft, bottomRight, topRight;
    f32 left, top, bottom, right;

    top = rect->top;
    left = rect->left;
    bottom = rect->bottom;
    right = rect->right;

    topLeft.x = left;
    topLeft.y = top;
    topLeft.z = 0.0f;
    bottomLeft.x = left;
    bottomLeft.y = bottom;
    bottomLeft.z = 0.0f;
    bottomRight.x = right;
    bottomRight.y = bottom;
    bottomRight.z = 0.0f;
    topRight.x = right;
    topRight.y = top;
    topRight.z = 0.0f;

    DrawSolidLine(&topLeft, &bottomLeft, width, color);
    DrawSolidLine(&bottomLeft, &bottomRight, width, color);
    DrawSolidLine(&bottomRight, &topRight, width, color);
    DrawSolidLine(&topRight, &topLeft, width, color);
}
