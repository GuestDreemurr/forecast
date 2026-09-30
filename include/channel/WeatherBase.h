#ifndef CHANNEL_WEATHER_BASE_H
#define CHANNEL_WEATHER_BASE_H
#include <types.h>
#include <nw4r/ut.h>
#include <revolution/MTX.h>

class ButtonGroup;
class LayoutButton;

// A text field or icon slot placed by a layout pane (size 0x24)
struct TextBox {
    LayoutButton* mPane;          // at 0x0
    f32 mX;                       // at 0x4
    f32 mY;                       // at 0x8
    f32 mWidth;                   // at 0xC
    f32 mHeight;                  // at 0x10
    f32 mScaleX;                  // at 0x14
    f32 mScaleY;                  // at 0x18
    nw4r::ut::Color mColor;       // at 0x1C
    nw4r::ut::Color mShadowColor; // at 0x20
};

// d_weather_base.cpp: base of the forecast panels that slide around on screen (size 0xA0)
class WeatherBase {
public:
    WeatherBase(const Vec2& pos, const Vec2& size, s32 type);
    virtual ~WeatherBase() {}
    virtual void Calc() = 0;
    virtual void Draw() = 0;
    virtual void SetPosition(const Vec2& pos, const f32& scale, const u8& visible, BOOL checkHover);
    virtual BOOL IsBusy() {
        return FALSE;
    }

    void DrawIcon(TextBox* box, u32 id, u8 alpha);
    void DrawIconLarge(TextBox* box, u32 id, u8 alpha);
    void DrawText(TextBox* box, const wchar_t* text, u8 alpha);

    ButtonGroup* mLayout;                    // at 0x4
    Vec2 mPos;                               // at 0x8
    Vec2 mSize;                              // at 0x10
    nw4r::ut::Rect mRect;                    // at 0x18
    nw4r::ut::TextWriterBase<wchar_t> mWriter; // at 0x28
    f32 mScale;                              // at 0x88
    s32 mType;                               // at 0x8C
    s32 unk90;                               // at 0x90
    s32 mAlpha;                              // at 0x94
    s32 unk98;                               // at 0x98
    u8 mCulled;                              // at 0x9C
    u8 mVisible;                             // at 0x9D
};

struct DayForecast;
struct WeatherType;

// A forecast's weather icon and its description
struct WeatherInfo {
    WeatherType* mType;   // at 0x0
    const wchar_t* mText; // at 0x4
};

// d_weather_base_day.cpp: a single day's forecast panel (size 0x380)
class WeatherBaseDay : public WeatherBase {
public:
    WeatherBaseDay(void* arc, const Vec2& pos, const Vec2& size, s32 type);
    virtual ~WeatherBaseDay();
    virtual void SetPosition(const Vec2& pos, const f32& scale, const u8& visible, BOOL checkHover);

    void SetupJP(void* arc);
    void Setup(void* arc);
    void DrawTextFit(TextBox* box, const wchar_t* text, u8 alpha);
    void DrawTempC(s8 temp, u8 alpha);
    void DrawTemp(s8 temp, u8 alpha);
    void DrawJP(DayForecast* day, WeatherInfo* weather, u8 alpha, f32 scale);
    void Draw(DayForecast* day, WeatherInfo* weather, u8 alpha, f32 scale);
    void FormatWind(u8 dir, u8 speed);
    void FormatWindDefault(u8 dir, u8 speed);
    void UpdateHover(BOOL enable);
    Vec2 GetIconPos();

    void PrintBox(TextBox* box, const wchar_t* text, u8 alpha);
    void DrawNumBox(TextBox* box, u8 alpha);
    void DrawTempCInline(s8 temp, u8 alpha);
    void FormatWindInline(u8 dir, u8 speed);

    ButtonGroup* mNoServiceLayout;  // at 0xA0, life_no_service.brlyt
    LayoutButton* mNoServiceText;   // at 0xA4
    TextBox* mIconBox;              // at 0xA8
    TextBox mBoxes[20];             // at 0xAC
    s32 mStartHour;                 // at 0x37C
};

#endif
