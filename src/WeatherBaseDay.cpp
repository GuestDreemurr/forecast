// d_weather_base_day.cpp: a single day's forecast panel
#include <channel/WeatherBase.h>
#include <channel/ColorWhite.h>
#include <channel/ForecastData.h>
#include <channel/LayoutButton.h>
#include <channel/SceneBase.h>
#include <channel/System.h>
#include <channel/WeatherScene.h>

#include <revolution/GX.h>
#include <wstring.h>

extern f32 gUnkSceneFloat;
extern wchar_t sTextBuf[0x100];
extern nw4r::ut::Font* gSysFont;

wchar_t* FormatNumber(s32 value, wchar_t* pBuf, s32 digits, BOOL zeroPad);
void DrawWeatherIcon(u16 icon, const Vec2* pos, u8 alpha, f32 scale);

static const f32 sBoxScalesJP[20] = {
    1.0f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.6f,
    0.7f, 1.0f, 0.55f, 1.0f, 0.55f, 0.65f, 0.65f, 0.65f, 0.65f, 1.0f,
};
static const s32 sRainBoxesJP[4] = {15, 16, 17, 18};
static const f32 sBoxScales[5] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

static const char* sBoxNamesJP[20] = {
    "telop",       "text_kion",   "text_rain_a", "text_rain_b", "text_rain_c", "text_rain_d", "text_kionH",
    "text_kionL",  "text_ondo",   "text_rain",   "text_percent", "kionH_1-100", "kionH_e",     "kionL_1-100",
    "kionL_e",     "rain_a_10",   "rain_b_10",   "rain_c_10",   "rain_d_10",   "icon",
};
static const char* sBoxNames[5] = {
    "telop",
    "wind_telop",
    "kion_doF_cnt",
    "kion_do_cnt",
    "icon",
};

static inline void SetColor(nw4r::ut::Color& dst, const Color& src) {
    dst.r = src.r;
    dst.g = src.g;
    dst.b = src.b;
    dst.a = src.a;
}

typedef const wchar_t* WindDirNames[17];
typedef const wchar_t* WindUnitNames[2];

static inline const wchar_t* GetWindDirName(u8 dir) {
    if (gLanguage > 6) {
        return ((WindDirNames*)gWindDirNamesUS)[1][dir];
    } else if (gRegion == 1) {
        return ((WindDirNames*)gWindDirNamesEU)[gLanguage][dir];
    } else {
        return ((WindDirNames*)gWindDirNamesUS)[gLanguage][dir];
    }
}

static inline const wchar_t* GetWindUnitName() {
    if (gRegion == 1) {
        return ((WindUnitNames*)gWindUnitNames)[gLanguage][gWindUnit];
    } else {
        return ((WindUnitNames*)gWindUnitNames2)[gLanguage][gWindUnit];
    }
}

WeatherBaseDay::WeatherBaseDay(void* arc, const Vec2& pos, const Vec2& size, s32 type)
    : WeatherBase(pos, size, type), mNoServiceLayout(NULL), mNoServiceText(NULL) {
    mStartHour = 0;
    if (gLanguage == 0) {
        SetupJP(arc);
    } else {
        Setup(arc);
    }
}

WeatherBaseDay::~WeatherBaseDay() {
    if (mNoServiceLayout != NULL) {
        delete mNoServiceLayout;
        mNoServiceLayout = NULL;
        mNoServiceText = NULL;
    }

    if (mLayout != NULL) {
        delete mLayout;
        mLayout = NULL;
    }
}

void WeatherBaseDay::SetupJP(void* arc) {
    static f32 sIconWidth = 180.0f;
    static f32 sIconHeight = 120.0f;
    char layoutName[0x100] = "day_nJP.brlyt";
    f32 centerX = 0.5f * GetScreenWidth();
    f32 centerY = 228.0f;
    f32 scaleX = gWidescreen ? 1.3684211f : 1.0f;

    mLayout = new ButtonGroup(arc, layoutName, gButtonColors, true);
    mNoServiceLayout = new ButtonGroup(arc, "life_no_service.brlyt", gButtonColors, false);
    mNoServiceText = mNoServiceLayout->FindButton("text");
    if (mNoServiceText == NULL) {
        OSPanic("d_weather_base_day.cpp", 178,
                "text \x82\xAA\x8C\xA9\x82\xC2\x82\xA9\x82\xE8\x82\xDC\x82\xB9\x82\xF1\x21\x21\n");
    }

    for (s32 i = 0; i < 20; i++) {
        TextBox* box = &mBoxes[i];
        box->mPane = mLayout->FindButton(sBoxNamesJP[i]);
        if (box->mPane == NULL) {
            OSReport("%s\x82\xAA\x8C\xA9\x82\xC2\x82\xA9\x82\xE8\x82\xDC\x82\xB9\x82\xF1\x21\x21\n", sBoxNamesJP[i]);
            OSPanic("d_weather_base_day.cpp", 189, "");
        }

        Vec2 center = box->mPane->GetCenter();
        box->mX = center.x;
        box->mX = centerX + center.x * scaleX;
        box->mY = centerY - center.y;
        box->mWidth = scaleX * (box->mPane->mRight - box->mPane->mLeft);
        box->mHeight = __fabs(box->mPane->mTop - box->mPane->mBottom);
        box->mScaleX = sBoxScalesJP[i];
        box->mScaleY = sBoxScalesJP[i];

        switch (i) {
        case 6:
            SetColor(box->mColor, gColorRed);
            break;
        case 7:
            SetColor(box->mColor, gColorCyan);
            break;
        case 11:
            SetColor(box->mColor, gColorPink);
            SetColor(box->mShadowColor, gColorRed2);
            break;
        case 12:
        case 14:
            SetColor(box->mColor, gColorLightGray);
            SetColor(box->mShadowColor, gColorDarkGray2);
            break;
        case 13:
            SetColor(box->mColor, gColorCyan2);
            SetColor(box->mShadowColor, gColorBlue);
            break;
        case 15:
        case 16:
        case 17:
        case 18:
            SetColor(box->mColor, gColorWhite2);
            SetColor(box->mShadowColor, gColorDarkGray3);
            break;
        default:
            SetColor(box->mColor, gColorWhite);
            SetColor(box->mShadowColor, gColorDarkGray);
            break;
        }
    }

    mIconBox = &mBoxes[19];
    mBoxes[19].mWidth = sIconWidth;
    mBoxes[19].mHeight = sIconHeight;
}

void WeatherBaseDay::Setup(void* arc) {
    static f32 sIconWidth = 180.0f;
    static f32 sIconHeight = 120.0f;
    char layoutName[0x100] = "day_nWW.brlyt";
    f32 centerX = 0.5f * GetScreenWidth();
    f32 centerY = 228.0f;
    f32 scaleX = gWidescreen ? 1.3684211f : 1.0f;

    mLayout = new ButtonGroup(arc, layoutName, gButtonColors, true);
    mNoServiceLayout = new ButtonGroup(arc, "life_no_service.brlyt", gButtonColors, false);
    mNoServiceText = mNoServiceLayout->FindButton("text");
    if (mNoServiceText == NULL) {
        OSPanic("d_weather_base_day.cpp", 252,
                "text \x82\xAA\x8C\xA9\x82\xC2\x82\xA9\x82\xE8\x82\xDC\x82\xB9\x82\xF1\x21\x21\n");
    }

    for (s32 i = 0; i < 5; i++) {
        TextBox* box = &mBoxes[i];
        box->mPane = mLayout->FindButton(sBoxNames[i]);
        if (box->mPane == NULL) {
            OSReport("%s\x82\xAA\x8C\xA9\x82\xC2\x82\xA9\x82\xE8\x82\xDC\x82\xB9\x82\xF1\x21\x21\n", sBoxNames[i]);
            OSPanic("d_weather_base_day.cpp", 263, "");
        }

        Vec2 center = box->mPane->GetCenter();
        box->mX = center.x;
        box->mX = centerX + center.x * scaleX;
        box->mY = centerY - center.y;
        box->mWidth = scaleX * (box->mPane->mRight - box->mPane->mLeft);
        box->mHeight = __fabs(box->mPane->mTop - box->mPane->mBottom);
        box->mScaleX = sBoxScales[i];
        box->mScaleY = sBoxScales[i];
        SetColor(box->mColor, gColorWhite);
        SetColor(box->mShadowColor, gColorDarkGray);
    }

    f32 margin = gWidescreen ? 36 : 28;
    mIconBox = &mBoxes[4];
    mBoxes[1].mX = GetScreenWidth() - margin;
    mBoxes[1].mWidth = mBoxes[1].mX - (20.0f + (mBoxes[0].mX + 0.5f * mBoxes[0].mWidth));
    mBoxes[4].mWidth = sIconWidth;
    mBoxes[4].mHeight = sIconHeight;
}

void WeatherBaseDay::SetPosition(const Vec2& pos, const f32& scale, const u8& visible, BOOL checkHover) {
    WeatherBase::SetPosition(pos, scale, visible, checkHover);

    mStartHour = 0;
    if (gCurrentCity != NULL) {
        if (gCurrentCity->mForecast != NULL) {
            mStartHour = 0;
        } else if (gCurrentCity->mSummary != NULL) {
            mStartHour = 0;
        }
    }

    if (!IsBusy()) {
        UpdateHover(checkHover);
    }
}

void WeatherBaseDay::DrawTextFit(TextBox* box, const wchar_t* text, u8 alpha) {
    Vec2 pos;
    f32 scale;

    pos.x = box->mX;
    pos.y = box->mY - mPos.y;
    mWriter.SetScale(box->mScaleX, box->mScaleY);

    f32 width = mWriter.CalcStringWidth(text);
    if (width > box->mWidth) {
        scale = (box->mScaleX * box->mWidth) / width;
    } else {
        scale = box->mScaleX;
    }

    mWriter.SetDrawFlag(0x122);
    mWriter.SetupGX();
    box->mShadowColor.a = alpha;
    box->mColor.a = alpha;
    mWriter.SetLineSpace(0.0f);
    mWriter.SetCharSpace(0.0f);
    mWriter.SetScale(scale, scale);
    mWriter.SetTextColor(box->mShadowColor);
    mWriter.SetCursor(1.0f + pos.x, 1.0f + pos.y);
    mWriter.Print(text);
    mWriter.SetTextColor(box->mColor);
    mWriter.SetCursor(pos.x, pos.y);
    mWriter.Print(text);
}

inline void WeatherBaseDay::DrawTempCInline(s8 temp, u8 alpha) {
    Vec2 pos;

    if (temp <= -128) {
        wcscpy(sTextBuf, L"--");
    } else {
        FormatNumber(temp, sTextBuf, 4, FALSE);
    }

    wcscat(sTextBuf, L"\xFF9F" L"C");
    pos.y = mBoxes[2].mY - mPos.y;
    pos.x = mBoxes[2].mX + mPos.x;
    mBoxes[2].mShadowColor.a = alpha;
    mBoxes[2].mColor.a = alpha;
    DrawTempCentered(sTextBuf, &pos, mBoxes[2].mScaleX, mBoxes[2].mScaleY, &mBoxes[2].mColor,
                     &mBoxes[2].mShadowColor);
}

void WeatherBaseDay::DrawTempC(s8 temp, u8 alpha) {
    DrawTempCInline(temp, alpha);
}

void WeatherBaseDay::DrawTemp(s8 temp, u8 alpha) {
    Vec2 pos;
    TextBox* box;

    if (temp <= -128) {
        wcscpy(sTextBuf, L"--");
    } else {
        FormatNumber(temp, sTextBuf, 4, FALSE);
    }

    s32 len = wcslen(sTextBuf);
    if (gRegion == 1) {
        wcscat(sTextBuf, L"\xFF9F");
        box = &mBoxes[3];
    } else {
        wcscat(sTextBuf, L"\xFF9F" L"F");
        box = &mBoxes[2];
    }

    pos.y = box->mY - mPos.y;
    pos.x = box->mX + mPos.x;
    box->mShadowColor.a = alpha;
    box->mColor.a = alpha;
    DrawTempCentered(sTextBuf, &pos, box->mScaleX, box->mScaleY, &box->mColor, &box->mShadowColor);
}

inline void WeatherBaseDay::PrintBox(TextBox* box, const wchar_t* text, u8 alpha) {
    box->mColor.a = alpha;
    mWriter.SetScale(box->mScaleX, box->mScaleY);
    mWriter.SetTextColor(box->mColor);
    mWriter.SetCursor(box->mX + mPos.x, box->mY - mPos.y);
    mWriter.Print(text);
}

static inline void FormatHours(s32 start, s32 end) {
    wchar_t* p = FormatNumber(start, sTextBuf, 4, FALSE);
    p[0] = L'-';
    p[1] = L'\0';
    p = FormatNumber(end, p + 1, 4, FALSE);
    p[0] = L'\x6642';
    p[1] = L'\0';
}

static inline void FormatTemp(s8 temp) {
    if (temp <= -128) {
        wcscpy(sTextBuf, L"--");
    } else {
        FormatNumber(temp, sTextBuf, 4, FALSE);
    }
}

static inline void FormatTempDiff(s8 diff) {
    if (diff <= -128) {
        wcscpy(sTextBuf, L"--");
    } else {
        wchar_t* p = sTextBuf;
        *p++ = L'(';
        if (diff > 0) {
            *p++ = L'+';
        }
        FormatNumber(diff, p, 4, FALSE);
        wcscat(sTextBuf, L")");
    }
}

inline void WeatherBaseDay::DrawNumBox(TextBox* box, u8 alpha) {
    Vec2 pos;
    pos.x = box->mX + mPos.x;
    pos.y = box->mY - mPos.y;
    box->mColor.a = alpha;
    box->mShadowColor.a = 0;
    DrawNumRightAligned(sTextBuf, &pos, box->mScaleX, box->mScaleY, &box->mColor, &box->mShadowColor);
}

void WeatherBaseDay::DrawJP(DayForecast* day, WeatherInfo* weather, u8 alpha, f32 scale) {
    s32 hour = mStartHour;
    Vec2 pos;
    pos.x = mBoxes[19].mX + mPos.x;
    pos.y = mBoxes[19].mY - mPos.y;

    f32 iconScale = scale * mBoxes[19].mScaleX;
    if ((weather->mType->mIcon & 0x7FFF) >= 100) {
        iconScale *= 0.8f;
    }
    DrawWeatherIcon(weather->mType->mIcon, &pos, alpha, iconScale);

    SetDefaultGXState();
    SetOrthoProjection();
    mWriter.SetFont(*gSysFont);
    mWriter.SetDrawFlag(0x111);
    mWriter.SetScale(scale);
    mWriter.SetCharSpace(scale * gUnkSceneFloat);
    mWriter.SetupGX();

    DrawText(&mBoxes[0], weather->mText, alpha);
    PrintBox(&mBoxes[1], L"\x6C17\x6E29 (\x524D\x65E5\x6BD4)", alpha);

    FormatHours(hour, hour + 6);
    PrintBox(&mBoxes[2], sTextBuf, alpha);
    FormatHours(hour + 6, hour + 12);
    PrintBox(&mBoxes[3], sTextBuf, alpha);
    FormatHours(hour + 12, hour + 18);
    PrintBox(&mBoxes[4], sTextBuf, alpha);
    s32 end = hour + 24;
    if (end > 24) {
        end -= 24;
    }
    FormatHours(hour + 18, end);
    PrintBox(&mBoxes[5], sTextBuf, alpha);

    PrintBox(&mBoxes[6], L"\x6700\x9AD8", alpha);
    PrintBox(&mBoxes[7], L"\x6700\x4F4E", alpha);
    if (gTempUnit == 0) {
        PrintBox(&mBoxes[8], L"(\xFF9F" L"C)", alpha);
    } else {
        PrintBox(&mBoxes[8], L"(\xFF9F" L"F)", alpha);
    }
    PrintBox(&mBoxes[9], L"\x964D\x6C34\x78BA\x7387", alpha);
    PrintBox(&mBoxes[10], L"(%)", alpha);

    SetDefaultGXState();
    SetOrthoProjection();
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C2, GX_CC_TEXC, GX_CC_C1, GX_CC_C2);

    FormatTemp(gTempUnit == 0 ? day->mMaxC : day->mMaxF);
    DrawNumBox(&mBoxes[11], alpha);
    FormatTempDiff(gTempUnit == 0 ? day->mMaxDiffC : day->mMaxDiffF);
    DrawNumBox(&mBoxes[12], alpha);
    FormatTemp(gTempUnit == 0 ? day->mMinC : day->mMinF);
    DrawNumBox(&mBoxes[13], alpha);
    FormatTempDiff(gTempUnit == 0 ? day->mMinDiffC : day->mMinDiffF);
    DrawNumBox(&mBoxes[14], alpha);

    SetDefaultGXState();
    SetOrthoProjection();
    for (s32 i = 0; i < 4; i++) {
        u8 percent = day->mPercent[i];
        if (percent == 0xFF) {
            wcscpy(sTextBuf, L"--");
        } else {
            FormatNumber(percent, sTextBuf, 4, FALSE);
        }

        s32 len = wcslen(sTextBuf);
        TextBox* box = &mBoxes[sRainBoxesJP[i]];
        pos.x = box->mX + mPos.x;
        pos.y = box->mY - mPos.y;
        box->mShadowColor.a = alpha;
        box->mColor.a = alpha;
        DrawDateCentered(sTextBuf, &pos, box->mScaleX, box->mScaleY, &box->mColor, &box->mShadowColor);
    }
}

inline void WeatherBaseDay::FormatWindInline(u8 dir, u8 speed) {
    wchar_t* p;

    switch (gLanguage) {
    case 6:
    case 4:
    case 3:
        wcscpy(sTextBuf, GetWindDirName(dir));
        if (dir != 0) {
            wcscat(sTextBuf, L", ");
            p = &sTextBuf[wcslen(sTextBuf)];
            if (speed != 0xFF) {
                FormatNumber(speed, p, 4, FALSE);
            } else {
                wcscat(sTextBuf, L"--");
            }
            wcscat(sTextBuf, L" ");
            wcscat(sTextBuf, GetWindUnitName());
        }
        break;
    case 5:
    case 2:
        if (dir == 0) {
            wcscpy(sTextBuf, GetWindDirName(dir));
        } else {
            wcscpy(sTextBuf, gLanguage == 5 ? L"Vento da " : L"Wind aus ");
            wcscat(sTextBuf, GetWindDirName(dir));
            wcscat(sTextBuf, L", ");
            p = &sTextBuf[wcslen(sTextBuf)];
            if (speed != 0xFF) {
                FormatNumber(speed, p, 4, FALSE);
            } else {
                wcscat(sTextBuf, L"--");
            }
            wcscat(sTextBuf, L" ");
            wcscat(sTextBuf, GetWindUnitName());
        }
        break;
    default:
        FormatWindDefault(dir, speed);
        break;
    }
}

void WeatherBaseDay::Draw(DayForecast* day, WeatherInfo* weather, u8 alpha, f32 scale) {
    Vec2 pos;
    pos.x = mBoxes[4].mX + mPos.x;
    pos.y = mBoxes[4].mY - mPos.y;
    DrawWeatherIcon(weather->mType->mIcon, &pos, alpha, scale * mBoxes[4].mScaleX);

    SetDefaultGXState();
    SetOrthoProjection();
    mWriter.SetFont(*gSysFont);
    mWriter.SetDrawFlag(0x111);
    mWriter.SetScale(scale);
    mWriter.SetCharSpace(scale * gUnkSceneFloat);
    mWriter.SetupGX();

    DrawText(&mBoxes[0], weather->mText, alpha);

    u8 dir = day->mWindDirection;
    if (dir != 0xFF) {
        u8 speed = gWindUnit == 0 ? day->mWindSpeedMph : day->mWindSpeedKmh;
        FormatWindInline(dir, speed);
        DrawTextFit(&mBoxes[1], sTextBuf, alpha);
    }

    SetDefaultGXState();
    SetOrthoProjection();
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C2, GX_CC_TEXC, GX_CC_C1, GX_CC_C2);
    if (gTempUnit == 0) {
        DrawTempCInline(day->mMaxC, alpha);
    } else {
        DrawTemp(day->mMaxF, alpha);
    }
}

void WeatherBaseDay::FormatWind(u8 dir, u8 speed) {
    FormatWindInline(dir, speed);
}

void WeatherBaseDay::FormatWindDefault(u8 dir, u8 speed) {
    if (gRegion == 1) {
        if (dir != 0) {
            if (speed != 0xFF) {
                swprintf(sTextBuf, 0x100, L"%ls %d %ls", GetWindDirName(dir), speed, GetWindUnitName());
            } else {
                swprintf(sTextBuf, 0x100, L"%ls -- %ls", GetWindDirName(dir), GetWindUnitName());
            }
        } else if (speed != 0xFF) {
            swprintf(sTextBuf, 0x100, L"%d %ls", speed, GetWindUnitName());
        } else {
            swprintf(sTextBuf, 0x100, L"-- %ls", GetWindUnitName());
        }
    } else {
        wcscpy(sTextBuf, GetWindDirName(dir));
        if (dir != 0) {
            wcscat(sTextBuf, L" ");
            wchar_t* p = &sTextBuf[wcslen(sTextBuf)];
            if (speed != 0xFF) {
                FormatNumber(speed, p, 4, FALSE);
            } else {
                wcscat(sTextBuf, L"--");
            }
            wcscat(sTextBuf, L" ");
            wcscat(sTextBuf, GetWindUnitName());
        }
    }
}

void WeatherBaseDay::UpdateHover(BOOL enable) {
    s32 prev = unk98;
    unk98 = -1;
    if (!enable) {
        return;
    }

    TextBox* icon = mIconBox;
    f32 left = (icon->mX + mPos.x) - 0.5f * icon->mWidth;
    f32 top = (icon->mY - mPos.y) - 0.5f * icon->mHeight;
    f32 right = left + icon->mWidth;
    f32 bottom = top + icon->mHeight;

    for (s32 i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        if (gKPADLatest[i] >= 0 && gCursorX[i] > left && gCursorX[i] < right && gCursorY[i] > top &&
            gCursorY[i] < bottom) {
            unk98 = i;
            break;
        }
    }

    if (unk98 >= 0) {
        if (mIconBox->mScaleX < 1.3f) {
            mIconBox->mScaleX += 0.05f;
            if (mIconBox->mScaleX >= 1.3f) {
                mIconBox->mScaleX = 1.3f;
            }
        }

        if (prev == -1) {
            StartRumble(unk98, 3, 20);
        }
    } else if (mIconBox->mScaleX > 1.0f) {
        mIconBox->mScaleX -= 0.05f;
        if (mIconBox->mScaleX <= 1.0f) {
            mIconBox->mScaleX = 1.0f;
        }
    }
}

Vec2 WeatherBaseDay::GetIconPos() {
    Vec2 pos;
    pos.y = mIconBox->mY - mPos.y;
    pos.x = mIconBox->mX + mPos.x;
    return pos;
}
