#ifndef CHANNEL_CITY_LABEL_H
#define CHANNEL_CITY_LABEL_H
#include <types.h>
#include <channel/Color.h>
#include <channel/DrawUtil.h>
#include <channel/Vec2F.h>
#include <channel/Vec3.h>
#include <channel/Vector2.h>
#include <nw4r/ut/ut_Rect.h>
#include <nw4r/ut/ut_TextWriterBase.h>

class City;
struct GlobeView;
namespace nw4r {
namespace ut {
class Font;
}
}

enum CityLabelFlag {
    CITY_LABEL_ON_SCREEN = (1 << 0),
    CITY_LABEL_HIDDEN = (1 << 1),
    CITY_LABEL_HOVERED = (1 << 2),
};

enum CityLabelFlag2 {
    CITY_LABEL_SELECTED = (1 << 0),
    CITY_LABEL_HIT = (1 << 1),
};

// The name and forecast of one city, drawn over the globe (size 0x2FC)
class CityLabel {
public:
    typedef void (CityLabel::*DrawFunc)();

    CityLabel(City* city, f32 scale, nw4r::ut::Font* font);
    ~CityLabel();

    void Project(GlobeView* view, s8 zoomLevel);
    void UpdateTempText();
    void UpdateHover();
    void UpdateAlpha();
    void ResetDrawFunc();
    void CalcSize();
    void DrawBg();
    void DrawName();
    void DrawTempAlt(s8 day, f32 scale);
    void DrawTemp(s8 day, f32 scale);
    void DrawRain(s32 day, f32 scale);
    void DrawHigh(f32 scale);
    void DrawDetail(BOOL tomorrow, f32 scale);
    void SetPosition(const Vec2* pos, f32 scale);
    BOOL IsDrawn();
    void UpdateEdgeFade();
    u16 GetWeatherCode(s8 day);
    Vec2F GetIconSize(s8 day);
    Vec2F GetTempSize(s8 day);
    Vec2F GetTempSizeSmall(s8 day);
    Vec2F GetTempSizeLarge(s8 day);

    Vec2F GetBoxPos() const {
        return mBoxPos;
    }

    Vec2F GetSize() const {
        return mSize;
    }

    CityLabel* mNext;          // at 0x0
    CityLabel* mNextBack;      // at 0x4
    CityLabel* mNextFront;     // at 0x8
    CityLabel* mNextFrontBack; // at 0xC
    CityLabel* mNextOverlap;   // at 0x10
    City* mCity;               // at 0x14
    u8 unk18[0xFC - 0x18];     // at 0x18
    nw4r::ut::TextWriterBase<wchar_t> mWriter; // at 0xFC
    nw4r::ut::Rect mBounds;    // at 0x15C
    Rect mBgRects[2];          // at 0x16C
    u8 unk18C[0x1F8 - 0x18C];  // at 0x18C
    Vector2 mPos;              // at 0x1F8
    u8 unk200[0x210 - 0x200];  // at 0x200
    Vec2F mSize;               // at 0x210
    u8 unk218[0x220 - 0x218];  // at 0x218
    Vec2F mBoxPos;             // at 0x220
    u8 unk228[0x230 - 0x228];  // at 0x228
    u32 mFlags;                // at 0x230
    u8 unk234[0x238 - 0x234];  // at 0x234
    u32 mInputFlags;           // at 0x238
    s32 mBounce;               // at 0x23C
    u8 unk240[0x250 - 0x240];  // at 0x240
    f32 mScale;                // at 0x250
    u8 unk254[0x27D - 0x254];  // at 0x254
    u8 mAlpha;                 // at 0x27D
    u8 unk27E[0x280 - 0x27E];  // at 0x27E
    u8 mPriority;              // at 0x280
    u8 unk281[0x2FC - 0x281];  // at 0x281
};

extern const s32 gLabelDetailZoom;
extern const f32 gLabelIconSize;

#endif
