#ifndef CHANNEL_CITY_LABEL_H
#define CHANNEL_CITY_LABEL_H
#include <types.h>
#include <channel/Color.h>
#include <channel/DrawUtil.h>
#include <channel/Vec3.h>
#include <channel/Vector2.h>
#include <nw4r/ut/ut_Rect.h>
#include <nw4r/ut/ut_TextWriterBase.h>

class City;
struct DayForecast;
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
    CITY_LABEL_POINTED = (1 << 3), // a pointer is inside the label's box
};

enum CityLabelFlag2 {
    CITY_LABEL_SELECTED = (1 << 0),
    CITY_LABEL_HIT = (1 << 1),
};

// The texts of one forecast day, each per temperature unit (size 0x66)
struct CityLabelDay {
    wchar_t mMax[2][5];     // at 0x0
    wchar_t mMaxDiff[2][5]; // at 0x14, change from the day before
    wchar_t mMin[2][5];     // at 0x28
    wchar_t mMinDiff[2][5]; // at 0x3C
    wchar_t mPercent[2][5]; // at 0x50, precipitation chances
    s8 mMaxC;               // at 0x64, -128 = none
};

// The name and forecast of one city, drawn over the globe (size 0x2FC)
class CityLabel {
public:
    typedef void (CityLabel::*DrawFunc)();
    typedef BOOL (CityLabel::*StateFunc)();

    CityLabel(City* city, f32 scale, nw4r::ut::Font* font);
    ~CityLabel();

    void Project(GlobeView* view, s32 zoomLevel);
    void UpdateTempText();
    void BuildDayText(DayForecast* days);
    wchar_t* FormatTemp(wchar_t* out, s32 temp);
    void UpdateBorderFade();
    void CheckPointer();
    void UpdateHover();
    void UpdateAlpha();
    void ResetDrawFunc();
    void UpdateBox();
    void CalcSize();
    void DrawBg();
    void DrawName();
    void DrawDayTemp(s32 day, f32 scale);
    void DrawTempAlt(s32 day, f32 scale);
    void DrawTemp(s32 day, f32 scale);
    void DrawRain(s32 day, f32 scale);
    void DrawHigh(f32 scale);
    void DrawDetail(BOOL tomorrow, f32 scale);
    nw4r::ut::Color GetTempColor(s8 temp);
    nw4r::ut::Color BlendColor(const Color& to, const Color& from, f32 duration, f32 time);
    void SetPosition(Vec2* pos, f32 scale);
    void UpdateBounds();
    BOOL IsDrawn();
    BOOL StateIdle();
    BOOL StateOpen();
    BOOL StateOpened();
    BOOL StateClose();
    void UpdateEdgeFade();

    void ChangeState(StateFunc state) {
        if (mState) {
            mStateStep = -1;
            (this->*mState)();
        }

        mState = state;
        mStateStep = 0;
        mStateTimer = 0;
        unk27A = 0;
        (this->*mState)();
    }

    void CallDrawFunc() {
        if (mDrawFunc) {
            (this->*mDrawFunc)();
        }
    }
    u16 GetWeatherCode(s8 day);
    Vector2 GetIconSize(s8 day);
    Vector2 GetTempSize(s8 day);
    Vector2 GetTempSizeSmall(s8 day);
    Vector2 GetTempSizeLarge(s8 day);

    Vector2 GetBoxPos() const {
        return mBoxPos;
    }

    Vector2 GetSize() const {
        return mSize;
    }

    CityLabel* mNext;          // at 0x0
    CityLabel* mNextBack;      // at 0x4
    CityLabel* mNextFront;     // at 0x8
    CityLabel* mNextFrontBack; // at 0xC
    CityLabel* mNextOverlap;   // at 0x10
    City* mCity;               // at 0x14
    CityLabelDay mDays[2];     // at 0x18, today and tomorrow
    wchar_t mNowText[2][5];    // at 0xE4, current temperature per unit
    s8 mNowC;                  // at 0xF8, -128 = none
    u8 unkF9[0xFC - 0xF9];     // at 0xF9
    nw4r::ut::TextWriterBase<wchar_t> mWriter; // at 0xFC
    Rect mBounds;              // at 0x15C
    Rect mBgRects[2];          // at 0x16C
    Color mShadowColor;        // at 0x18C
    Color mBgGradient0[4];     // at 0x190, corner colors of mBgRects[0]
    Color mBgGradient1[4];     // at 0x1A0, corner colors of mBgRects[1]
    StateFunc mState;          // at 0x1B0
    DrawFunc mDrawFunc;        // at 0x1BC, called by the states; UpdateBox by default
    Vec mWorldPos;             // at 0x1C8, the city on the globe
    Vec3 unk1D4[3];            // at 0x1D4
    Vec2 mPos;                 // at 0x1F8
    Vec2 mBoxTarget;           // at 0x200, where the open box goes, beside the label
    Vec2 mLabelPos;            // at 0x208, set by SetPosition
    Vector2 mSize;             // at 0x210
    Vec2 mOpenPos;             // at 0x218, where the label was when it opened
    Vector2 mBoxPos;           // at 0x220
    f32 mFontWidth;            // at 0x228
    f32 mFontHeight;           // at 0x22C
    u32 mFlags;                // at 0x230
    u32 mPrevFlags;            // at 0x234, mFlags of the previous frame
    u32 mInputFlags;           // at 0x238
    s32 mBounce;               // at 0x23C
    f32 mTomorrowWidth;        // at 0x240, width of the "tomorrow" caption
    f32 mMinWidth;             // at 0x244
    f32 mOpen;                 // at 0x248, 0 when closed, 1 when open
    f32 mTextScale;            // at 0x24C, set by SetPosition; IsDrawn hides the label while it is ~0
    f32 mScale;                // at 0x250
    f32 mOpenAngle;            // at 0x254, 0 to 90 degrees, mOpen is its sine
    f32 mZoomFade;             // at 0x258, eases to 1 while the zoom level shows this city
    f32 mEdgeFade;             // at 0x25C, dims the label near the top and bottom of the screen
    f32 mColorR;               // at 0x260
    f32 mColorG;               // at 0x264
    f32 mColorB;               // at 0x268
    f32 mFade;                 // at 0x26C
    f32 mHighlight;            // at 0x270, eases to 1 while a pointer is on the box
    f32 mBgAlpha;              // at 0x274, eases to 1 while hovered
    s16 mStateTimer;           // at 0x278
    s16 unk27A;                // at 0x27A
    s8 mStateStep;             // at 0x27C, 0 on entering a state, -1 on leaving it
    u8 mAlpha;                 // at 0x27D
    u8 unk27E;                 // at 0x27E
    u8 mAlphaLow;              // at 0x27F
    u8 mPriority;              // at 0x280
    u8 unk281;                 // at 0x281
    wchar_t mTempText[2][30];  // at 0x282, "max/min" for today and tomorrow
    u8 unk2FA[0x2FC - 0x2FA];  // at 0x2FA
};

extern const s32 gLabelDetailZoom;
extern const f32 gLabelIconSize;

#endif
