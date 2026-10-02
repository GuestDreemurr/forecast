// d_weather_normal.cpp: the main forecast display
#include <channel/WeatherViews.h>
#include <channel/ColorWhite.h>
#include <channel/DrawUtil.h>
#include <channel/ForecastData.h>
#include <channel/LayoutButton.h>
#include <channel/SceneBase.h>
#include <channel/SimpleGlobe.h>
#include <channel/System.h>
#include <channel/WeatherScene.h>

#include <nw4r/lyt.h>
#include <nw4r/math.h>
#include <revolution/OS.h>
#include <cstring>
#include <cstdio>
#include <wstring.h>

extern nw4r::ut::Font* gSysFont;
extern nw4r::ut::TextWriterBase<wchar_t> gTextWriter;
extern f32 gUnkSceneFloat;
extern s32 gCursorState[4];
extern Vector2 gCityPos;
extern u8 gShowAmbientSound;
extern s32 gForecastPage;
extern s32 gForecastPageCount;
extern char sNameBuf[0x100];
extern wchar_t sTextBuf[0x100];
extern const wchar_t* gUpdatedPrefixes[];
extern const wchar_t* gLastUpdatedPrefixes[];
extern const wchar_t* gAmText;
extern const wchar_t* gPmText;

void DrawWeatherIcon(u16 icon, const Vec2* pos, f32 scale, s32 alpha);
wchar_t* FormatNumber(s32 value, wchar_t* pBuf, s32 digits, BOOL zeroPad);
void MinutesToCalendarTime(u32 minutes, OSCalendarTime* cal);
void WrapHour(s32* pHour);
void WrapHourTo24(s32* pHour);
void RequestWeatherSounds(u32 type, f32 volume);
// Declared returning double here (the calls are followed by frsp), unlike in d_weather_around
double EaseCos(u16 t);
f32 SmoothApproach(f32* value, f32 target, f32 rate, f32 maxStep, f32 minStep);

// Zero-initialized statics live in .sdata here, not .sbss
#pragma explicit_zero_data on

static const WeatherNormal::Func sSetupDateFuncs[7] = {
    &WeatherNormal::SetupDateJP, &WeatherNormal::SetupDate, &WeatherNormal::SetupDate, &WeatherNormal::SetupDate,
    &WeatherNormal::SetupDate,   &WeatherNormal::SetupDate, &WeatherNormal::SetupDate,
};

static const WeatherNormal::Func sSetupBeltFuncs[7] = {
    &WeatherNormal::SetupBeltJP, &WeatherNormal::SetupBelt, &WeatherNormal::SetupBelt, &WeatherNormal::SetupBelt,
    &WeatherNormal::SetupBelt,   &WeatherNormal::SetupBelt, &WeatherNormal::SetupBelt,
};

static const f32 sBoxScalesJP[8] = {0.9f, 0.9f, 0.9f, 0.9f, 0.8f, 0.8f, 0.8f, 0.8f};
static const f32 sBoxScales[8] = {0.65f, 0.65f, 0.65f, 0.65f, 0.6f, 0.6f, 0.6f, 0.6f};

// Vertical offset of the city name while it scrolls through its lines
static const f32 sNameScrollY[4] = {0.0f, -50.0f, -100.0f, 0.0f};

static const u32 sUnusedColors0[10] = {
    0xD8D8D8FF, 0xFF7800FF, 0xF8BE00FF, 0x00A2DEFF, 0x47C528FF,
    0xD8D8D8FF, 0xD8D8D8FF, 0xD8D8D8FF, 0xFFFFFFFF, 0xFFFFFFFF,
};

static const u32 sUnusedColors1[10] = {
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x50FF32FF,
};

static const char* sBoxNamesJP[8] = {
    "icon_a", "icon_b", "icon_c", "icon_d", "text_icon_a", "text_icon_b", "text_icon_c", "text_icon_d",
};

static const char* sBoxNames[8] = {
    "icon_a", "icon_b", "icon_c", "icon_d", "text_icon_aUSA", "text_icon_bUSA", "text_icon_cUSA", "text_icon_dUSA",
};

#define CHANGE_STATE(state) SetState(state)

inline void WeatherNormal::SetState(StateFunc state) {
    if (mState) {
        mPhase = -1;
        (this->*mState)(0);
    }
    mState = state;
    mPhase = 0;
    if (mState) {
        (this->*mState)(0);
    }
}

#define CHANGE_SCROLL(state) ChangeScroll(state)

inline void WeatherNormal::ChangeScroll(Func state) {
    if (!IsScrollState(state)) {
        if (mScrollState) {
            mScrollPhase = -1;
            (this->*mScrollState)();
        }
        mScrollPhase = 0;
        mScrollState = state;
        if (mScrollState) {
            (this->*mScrollState)();
        }
    }
}

static inline s32 GetSideMargin() {
    return gWidescreen ? 36 : 28;
}

static inline f32 GetPaneX(ButtonPane* pane, f32 scale) {
    return (pane->mFlag & 4) ? scale * pane->mTransX : pane->mTransX;
}

static inline BOOL IsNearZero(f32 value) {
    BOOL result = FALSE;
    if (value < 0.0008f && value > -0.0008f) {
        result = TRUE;
    }
    return result;
}

WeatherNormal::WeatherNormal(void* arc)
    : mLayout(NULL), mBaseLayout(NULL), mBeltLayout(NULL), mTimeLayout(NULL), mAroundButton(NULL), mSetButton(NULL),
      mBackButton(NULL), mUpButton(NULL), mDownButton(NULL), mState(NULL), unk174(NULL), mSetupDate(NULL),
      mSetupBelt(NULL), mFormatDate(NULL), mScrollState(NULL), mPageX(0.0f), mPageY(0.0f), unk734(0.0f),
      unk738(0.0f), unk73C(0.0f), mFlashX(0.0f), mFlashY(0.0f), mZoomX(0.0f), mZoomY(0.0f), mDateX(0.0f),
      mDateY(0.0f), mDateWidth(0.0f), mDateHeight(0.0f), mDateScaleX(0.6f), mDateScaleY(0.6f), mWideOffset(0.0f),
      unk7C4(1.0f), unk7C8(1.0f), mAlpha(1.0f), mNameScaleY(1.0f), mNameScroll(0.0f), mTimesAlpha(0.0f),
      mFlash(FALSE), mShowZoom(FALSE), mUpPressed(FALSE), mDownPressed(FALSE), mActive(TRUE), mZoomed(FALSE),
      mSoundEnabled(FALSE), unk7FC(TRUE), mPhase(0), mFlashAlpha(0), mZoomAlpha(0), mDateType(-1), mBeltAlpha(255),
      mBeltTarget(255), mDateAlpha(255), mDateTarget(255), mBeltTimer(240), mBeltPhase(0), mBeltState(0),
      mTodayDay(0), mTomorrowDay(0), mNowDay(0), mForecastDay(0), mForecastDay2(0), mNumNames(0), mScrollPhase(0),
      mNameIndex(0), mScrollTimer(0), mAnimTimer(0), unk858(0) {
    gForecastPage = 1;

    mLayout = new ButtonGroup(arc, "forecast.brlyt", gButtonColors, false);
    mBaseLayout = new ButtonGroup(arc, "base.brlyt", gButtonColors, false);
    mBeltLayout = new ButtonGroup(arc, "base_belt.brlyt", gButtonColors, false);
    if (gLanguage == 0) {
        mTimeLayout = new ButtonGroup(arc, "day_6h_nJP.brlyt", gButtonColors, false);
    } else {
        mTimeLayout = new ButtonGroup(arc, "day_6h_nWW.brlyt", gButtonColors, false);
    }

    {
        s32 width = GetScreenWidth();
        f32 height = 456.0f;
        Vec2 pos;
        Vec2 offset;
        offset.x = 0.0f;
        pos.y = 0.5f * height;
        offset.y = 0.0f;
        pos.x = 0.5f * width;

        if (gLanguage == 0) {
            mPages[0] = new WeatherOther(pos, arc, offset, 0);
            offset.y += height;
            pos.y += height;
            mPages[1] = new WeatherToday(pos, arc, offset, 2);
            offset.y += height;
            pos.y += height;
            mPages[2] = new WeatherTomorrow(pos, arc, offset, 3);
            offset.y += height;
            pos.y += height;
            mPages[3] = new WeatherWeek(pos, arc, offset, 4);
        } else {
            mPages[0] = new WeatherOther(pos, arc, offset, 0);
            offset.y += height;
            pos.y += height;
            mPages[1] = new WeatherNow(pos, arc, offset, 1);
            offset.y += height;
            pos.y += height;
            mPages[2] = new WeatherToday(pos, arc, offset, 2);
            offset.y += height;
            pos.y += height;
            mPages[3] = new WeatherTomorrow(pos, arc, offset, 3);
            offset.y += height;
            pos.y += height;
            mPages[4] = new WeatherWeek(pos, arc, offset, 4);
        }
    }

    mAroundButton = mLayout->FindButton("around");
    if (mAroundButton == NULL) {
        OSPanic("d_weather_normal.cpp", 268,
                "aroundがありません!\n");
    }
    mSetButton = mLayout->FindButton("set");
    if (mSetButton == NULL) {
        OSPanic("d_weather_normal.cpp", 273,
                "setがありません!\n");
    }
    mBackButton = mLayout->FindButton("back");
    if (mBackButton == NULL) {
        OSPanic("d_weather_normal.cpp", 278,
                "backがありません!\n");
    }
    mUpButton = mLayout->FindButton("up");
    if (mUpButton == NULL) {
        OSPanic("d_weather_normal.cpp", 283,
                "upがありません!\n");
    }
    mDownButton = mLayout->FindButton("down");
    if (mDownButton == NULL) {
        OSPanic("d_weather_normal.cpp", 288,
                "downがありません!\n");
    }

    LayoutButton* city = mBeltLayout->FindButton("city");
    f32 centerX = 0.5f * GetScreenWidth();
    f32 centerY = 228.0f;
    f32 scaleX = gWidescreen ? 1.3684211f : 1.0f;
    if (city != NULL) {
        f32 w = city->mRight - city->mLeft;
        f32 h = __fabsf(city->mTop - city->mBottom);
        Vec2F center = city->GetCenter();
        f32 scaledW = w * scaleX;
        center.x *= scaleX;
        mCityRect.left = (centerX + center.x) - 0.5f * scaledW;
        mCityRect.right = mCityRect.left + scaledW;
        mCityRect.top = (centerY - center.y) - 0.5f * h;
        mCityRect.bottom = mCityRect.top + h;
    }

    if (gLanguage == 0) {
        strcpy(sNameBuf, "timeJP");
    } else {
        strcpy(sNameBuf, "timeWW");
    }
    LayoutButton* time = mBeltLayout->FindButton(sNameBuf);
    if (time != NULL) {
        Vec2F center = time->GetCenter();
        mDateX = center.x;
        mDateY = centerY - center.y;
        mDateHeight = __fabsf(time->mTop - time->mBottom);
        if (gLanguage == 0) {
            mDateX = centerX + mDateX * scaleX;
            mDateWidth = time->mRight - time->mLeft;
        } else {
            mDateX = GetScreenWidth() - GetSideMargin();
            mDateWidth = scaleX * (time->mRight - time->mLeft);
        }
    }

    LayoutButton* beltTime = mBeltLayout->FindButton("belt_time");
    if (beltTime != NULL) {
        beltTime->ShowLanguagePane("belt_timeB");
        if (gLanguage == 0) {
            f32 x = scaleX * ((beltTime->mRight + beltTime->mLeft) * 0.5f);
            strcpy(sNameBuf, "belt_timeB");
            ButtonPane* pane = (ButtonPane*)beltTime->FindPane(sNameBuf);
            if (pane != NULL) {
                x += GetPaneX(pane, scaleX);
                strcat(sNameBuf, GetLanguageCode());
                pane = (ButtonPane*)beltTime->FindPane(sNameBuf);
                if (pane != NULL) {
                    x += GetPaneX(pane, scaleX);
                    strcat(sNameBuf, "1");
                    pane = (ButtonPane*)beltTime->FindPane(sNameBuf);
                    if (pane != NULL) {
                        x += GetPaneX(pane, scaleX);
                    }
                }
            }
            mDateX = centerX + x;
        }
    }

    if (gWidescreen) {
        mBelt[0].mRect.left = 0.0f;
        mBelt[0].mRect.right = 240.0f;
        mBelt[1].mRect.left = 240.0f;
        mBelt[1].mRect.right = 832.0f;
    } else {
        mBelt[0].mRect.left = 0.0f;
        mBelt[0].mRect.right = 240.0f;
        mBelt[1].mRect.left = 240.0f;
        mBelt[1].mRect.right = 608.0f;
    }

    {
        f32 top = 74.0f;
        f32 bottom = 124.0f;
        f32 halfHeight = 0.5f * (bottom - top);
        mBelt[1].mRect.top = top;
        mBelt[0].mRect.top = top;
        mBelt[1].mRect.bottom = bottom;
        mBelt[0].mRect.bottom = bottom;
        mBelt[0].mX = 0.5f * (mBelt[0].mRect.right - mBelt[0].mRect.left) + mBelt[0].mRect.left;
        mBelt[0].mY = halfHeight + top;
        mBelt[0].mMaxWidth = (mBelt[0].mRect.right - GetSideMargin()) - 8.0f;
        mBelt[0].mMaxHeight = 2.0f * (halfHeight - 8.0f);

        halfHeight = 0.5f * (mBelt[1].mRect.bottom - mBelt[1].mRect.top);
        mBelt[1].mY = halfHeight + mBelt[1].mRect.top;
        mBelt[1].mX = GetScreenWidth() - GetSideMargin();
        mBelt[1].mMaxHeight = 2.0f * (halfHeight - 8.0f);
        mBelt[1].mMaxWidth = (mBelt[1].mX - mBelt[1].mRect.left) - 8.0f;
    }

    mSetupDate = sSetupDateFuncs[gLanguage];
    mSetupBelt = sSetupBeltFuncs[gLanguage];

    if (gLanguage == 0) {
        mUpdateBeltText = &WeatherNormal::UpdateBeltTextJP;
        SetupBoxesJP();
        mDrawTimes = &WeatherNormal::DrawTimesJP;
    } else {
        mUpdateBeltText = &WeatherNormal::UpdateBeltText;
        SetupBoxes();
        switch (gLanguage) {
        case 1:
            if (gRegion == 1) {
                mFormatDate = &WeatherNormal::FormatDateUS;
                mDrawTimes = &WeatherNormal::DrawTimesUS;
            } else {
                mFormatDate = &WeatherNormal::FormatDateEU;
                mDrawTimes = &WeatherNormal::DrawTimesEU;
            }
            break;
        case 2:
            mFormatDate = &WeatherNormal::FormatDateDE;
            mDrawTimes = &WeatherNormal::DrawTimesDE;
            break;
        case 3:
            if (gRegion == 1) {
                mFormatDate = &WeatherNormal::FormatDateCA;
            } else {
                mFormatDate = &WeatherNormal::FormatDateFR;
            }
            mDrawTimes = &WeatherNormal::DrawTimesFR;
            break;
        case 4:
            mFormatDate = &WeatherNormal::FormatDateES;
            mDrawTimes = &WeatherNormal::DrawTimesES;
            break;
        case 5:
            mFormatDate = &WeatherNormal::FormatDateIT;
            mDrawTimes = &WeatherNormal::DrawTimesIT;
            break;
        case 6:
            mFormatDate = &WeatherNormal::FormatDateNL;
            mDrawTimes = &WeatherNormal::DrawTimesNL;
            break;
        default:
            mFormatDate = &WeatherNormal::FormatDateEU;
            mDrawTimes = &WeatherNormal::DrawTimesEU;
            break;
        }
    }

    mZoomColors[0].r = 0;
    mZoomColors[0].g = 192;
    mZoomColors[0].b = 255;
    mZoomColors[0].a = 255;
    mZoomColors[1].r = 0;
    mZoomColors[1].g = 96;
    mZoomColors[1].b = 212;
    mZoomColors[1].a = 255;
    mZoomColors[2].r = 0;
    mZoomColors[2].g = 0;
    mZoomColors[2].b = 120;
    mZoomColors[2].a = 255;
    mZoomColors[3].r = 0;
    mZoomColors[3].g = 0;
    mZoomColors[3].b = 152;
    mZoomColors[3].a = 255;

    CHANGE_SCROLL(&WeatherNormal::ScrollNames);
    CHANGE_STATE(&WeatherNormal::StateNormal);
    mCalcFunc = &WeatherNormal::DrawCity;
    mWideOffset = 0.5f * (GetScreenWidth() - SCREEN_WIDTH_4_3);
}

WeatherNormal::~WeatherNormal() {
    for (s32 i = 0; i < gForecastPageCount; i++) {
        if (mPages[i] != NULL) {
            delete mPages[i];
        }
    }
    if (mTimeLayout != NULL) {
        delete mTimeLayout;
    }
    if (mBeltLayout != NULL) {
        delete mBeltLayout;
    }
    if (mBaseLayout != NULL) {
        delete mBaseLayout;
    }
    if (mLayout != NULL) {
        delete mLayout;
    }
}

#define SETUP_BOXES(names, scales, line)                                                                     \
    {                                                                                                        \
        s32 width = GetScreenWidth();                                                                        \
        f32 centerX = 0.5f * width;                                                                          \
        f32 centerY = 228.0f;                                                                                \
        f32 scaleX = gWidescreen ? 1.3684211f : 1.0f;                                                        \
        TextBox* box = mBoxes;                                                                               \
        for (s32 i = 0; i < 8; i++, box++) {                                                                 \
            box->mPane = mTimeLayout->FindButton(names[i]);                                                  \
            if (box->mPane == NULL) {                                                                        \
                OSReport("%sが見つかりません!!\n",     \
                         names[i]);                                                                          \
                OSPanic("d_weather_normal.cpp", line, "");                                                   \
            }                                                                                                \
            Vec2F center = box->mPane->GetCenter();                                                          \
            box->mX = center.x;                                                                              \
            box->mY = center.y;                                                                              \
            box->mX = centerX + box->mX * scaleX;                                                            \
            box->mY = centerY - box->mY;                                                                     \
            LayoutButton* pane = box->mPane;                                                                 \
            f32 w = pane->mRight - pane->mLeft;                                                              \
            f32 h = __fabsf(pane->mTop - pane->mBottom);                                                     \
            box->mWidth = scaleX * w;                                                                        \
            box->mHeight = h;                                                                                \
            box->mScaleX = scales[i];                                                                        \
            box->mScaleY = scales[i];                                                                        \
            box->mColor.r = gColorWhite.r;                                                                   \
            box->mColor.g = gColorWhite.g;                                                                   \
            box->mColor.b = gColorWhite.b;                                                                   \
            box->mColor.a = gColorWhite.a;                                                                   \
            box->mShadowColor.r = gColorDarkGray.r;                                                          \
            box->mShadowColor.g = gColorDarkGray.g;                                                          \
            box->mShadowColor.b = gColorDarkGray.b;                                                          \
            box->mShadowColor.a = gColorDarkGray.a;                                                          \
        }                                                                                                    \
    }

void WeatherNormal::SetupBoxesJP() {
    SETUP_BOXES(sBoxNamesJP, sBoxScalesJP, 560);
}

void WeatherNormal::SetupBoxes() {
    SETUP_BOXES(sBoxNames, sBoxScales, 585);
}

void WeatherNormal::Reset() {
    mLayout->Reset();
    mBaseLayout->Reset();
    mBeltLayout->Reset();
    mTimeLayout->Reset();
    for (s32 i = 0; i < gForecastPageCount; i++) {
        mPages[i]->Reset();
    }
    if (mCalcFunc) {
        (this->*mCalcFunc)();
    }
}

void WeatherNormal::Calc() {
    if (mCalcFunc) {
        (this->*mCalcFunc)();
    }
}

void WeatherNormal::Open() {
    mActive = TRUE;
    mCalcFunc = &WeatherNormal::CalcActive;
}

#define UPDATE_BACK_BUTTON()                                                                                 \
    if (gShowAmbientSound) {                                                                                 \
        mAroundButton->Unlock();                                                                             \
        mSetButton->Unlock();                                                                                \
        mBackButton->SetChildVisible("backI", FALSE);                                                        \
        mBackButton->SetState(0);                                                                            \
    } else {                                                                                                 \
        mAroundButton->Lock();                                                                               \
        mSetButton->Lock();                                                                                  \
        mBackButton->SetChildVisible("backI", TRUE);                                                         \
        mBackButton->SetState(1);                                                                            \
    }

void WeatherNormal::Show() {
    mLayout->ReleaseAll();
    mAlpha = 1.0f;
    CHANGE_STATE(&WeatherNormal::StateNormal);
    UPDATE_BACK_BUTTON();
}

void WeatherNormal::CalcActive() {
    unk7FC = TRUE;
    mSoundEnabled = TRUE;
    UpdateBeltHover();
    mUpPressed = FALSE;
    mDownPressed = FALSE;
    UPDATE_BACK_BUTTON();

    mLayout->FadeIn(15);
    mLayout->Calc();
    mBaseLayout->Calc();
    mBeltLayout->Calc();
    mTimeLayout->Calc();

    if (mZoomed) {
        if (mState) {
            (this->*mState)(0);
        }
    } else {
        UpdateButtons(mLayout, 40);

        if (!mUpButton->mToggle) {
            if (CheckButtonPressed("up", WPAD_BUTTON_A) >= 0) {
                mUpPressed = TRUE;
            } else if (gTrigAll & WPAD_BUTTON_UP) {
                mUpPressed = TRUE;
                mUpButton->Press(TRUE);
            }
        }

        if (!mDownButton->mToggle) {
            if (CheckButtonPressed("down", WPAD_BUTTON_A) >= 0) {
                mDownPressed = TRUE;
            } else if (gTrigAll & WPAD_BUTTON_DOWN) {
                mDownPressed = TRUE;
                mDownButton->Press(TRUE);
            }
        }

        if (CheckButtonPressed("set", WPAD_BUTTON_A) >= 0) {
            PlaySE(37);
            gSettingResult = 4;
            mSetButton->mPressed = TRUE;
            return;
        }

        if (CheckButtonPressed("back", WPAD_BUTTON_A) >= 0) {
            if (gShowAmbientSound) {
                PlaySE(37);
                gReturnToMenuRequested = TRUE;
                return;
            }
            PlaySE(38);
            gSettingResult = 3;
            CHANGE_STATE(&WeatherNormal::StateToAround);
            mBackButton->mPressed = TRUE;
        } else if (CheckButtonPressed("around", WPAD_BUTTON_A) >= 0) {
            PlaySE(0);
            gSettingResult = 5;
            CHANGE_STATE(&WeatherNormal::StateAroundWait);
            mAroundButton->mPressed = TRUE;
        } else if (mState) {
            (this->*mState)(0);
        }
    }

    if (mScrollState) {
        (this->*mScrollState)();
    }
    UpdateBelt();

    for (s32 i = 0; i < gForecastPageCount; i++) {
        mPages[i]->SetPosition(mPagePos[i], mAlpha, mPageVisible[i], !mZoomed);
    }

    gCursorState[0] = 0;
    gCursorState[1] = 0;
    gCursorState[2] = 0;
    gCursorState[3] = 0;
}

void WeatherNormal::DrawCity() {
    unk7FC = FALSE;
    mSoundEnabled = FALSE;
    mBelt[0].mHovered = FALSE;
    mBelt[1].mHovered = FALSE;
    mLayout->Calc();
    mBaseLayout->Calc();
    mBeltLayout->Calc();
    mTimeLayout->Calc();
    if (mState) {
        (this->*mState)(0);
    }
    if (mScrollState) {
        (this->*mScrollState)();
    }
    UpdateBelt();

    for (s32 i = 0; i < gForecastPageCount; i++) {
        mPages[i]->SetPosition(mPagePos[i], mAlpha, mPageVisible[i], FALSE);
    }
}

#define FIT_NAME(line)                                                                                       \
    (line).mScale = 1.0f;                                                                                    \
    gTextWriter.SetScale(1.0f);                                                                              \
    {                                                                                                        \
        f32 space = gUnkSceneFloat;                                                                          \
        gTextWriter.SetCharSpace((line).mScale * space);                                                     \
        f32 width = gTextWriter.CalcStringWidth((line).mText);                                               \
        if (width > maxWidth) {                                                                              \
            (line).mScale *= maxWidth / width;                                                               \
        }                                                                                                    \
    }

void WeatherNormal::SetCity() {
    City* city = gCurrentCity;
    mDateText[0] = 0;
    mDateType = 0;
    mNowIcon = 0xFFFF;
    mTodayIcon = 0xFFFF;
    mTomorrowIcon = 0xFFFF;

    if (city != NULL) {
        CityInfo* info = city->mInfo;
        gTextWriter.SetFont(*gSysFont);
        if (mSetupDate) {
            (this->*mSetupDate)();
        }

        if (info != NULL) {
            mNumNames = 0;
            f32 maxWidth = mCityRect.right - mCityRect.left;
            if (gLanguage == 0 && (info->GetId() & 0xFF000000) == 0x01000000) {
                mNumNames = 1;
                if (info->mRegion != NULL) {
                    wcscpy(mNames[0].mText, info->mRegion);
                    wcscat(mNames[0].mText, info->mName);
                } else {
                    wcscpy(mNames[0].mText, info->mName);
                }
                FIT_NAME(mNames[0]);

                if (info->mCountry != NULL) {
                    mNumNames++;
                    wcscpy(mNames[1].mText, info->mCountry);
                    FIT_NAME(mNames[1]);
                }
            } else {
                mNumNames++;
                wcscpy(mNames[0].mText, info->mName);
                FIT_NAME(mNames[0]);

                CityNameLine* line = &mNames[1];
                if (info->mRegion != NULL) {
                    mNumNames++;
                    wcscpy(line->mText, info->mRegion);
                    FIT_NAME(*line);
                    line++;
                }

                if (info->mCountry != NULL) {
                    mNumNames++;
                    wcscpy(line->mText, info->mCountry);
                    FIT_NAME(*line);
                }
            }
        }
    }
}

#define FIT_DATE(scaleX, scaleY)                                                                             \
    mDateScaleX = scaleX;                                                                                    \
    mDateScaleY = scaleY;                                                                                    \
    gTextWriter.SetScale(mDateScaleX, mDateScaleY);                                                          \
    f32 space = gUnkSceneFloat;                                                                              \
    gTextWriter.SetCharSpace(mDateScaleX * space);                                                           \
    {                                                                                                        \
        f32 width = gTextWriter.CalcStringWidth(mDateText);                                                  \
        if (width > mDateWidth) {                                                                            \
            mDateScaleX *= mDateWidth / width;                                                               \
        }                                                                                                    \
    }

static f32 sDateScaleXJP = 0.6f;
static f32 sDateScaleYJP = 0.6f;

void WeatherNormal::SetupDateJP() {

    if (gCurrentCity != NULL) {
        CityForecast* forecast = gCurrentCity->mForecast;
        CitySummary* summary = gCurrentCity->mSummary;
        u32 time;
        u16 today;
        u16 tomorrow;

        if (forecast != NULL) {
            mDateType = 0;
            ForecastEntry* entry = forecast->mEntry;
            time = entry->mTime;
            today = entry->mDays[0].mWeather;
            tomorrow = entry->mDays[1].mWeather;
        } else if (summary != NULL) {
            mDateType = 1;
            SummaryEntry* entry = summary->mEntry;
            time = entry->mTime;
            today = entry->mDays[0].mWeather;
            tomorrow = entry->mDays[1].mWeather;
        } else {
            return;
        }

        WeatherInfo* info = gForecastData->FindWeatherInfo(today);
        if (info != NULL) {
            mTodayIcon = info->mType->mIcon;
        }
        info = gForecastData->FindWeatherInfo(tomorrow);
        if (info != NULL) {
            mTomorrowIcon = info->mType->mIcon;
        }

        gTextWriter.SetFont(*gSysFont);
        OSCalendarTime cal;
        MinutesToCalendarTime(time, &cal);
        mTodayDay = cal.mday - 1;
        if (time != 0) {
            wchar_t* p = FormatNumber(cal.mday, mDateText, 2, FALSE);
            p[0] = 0x65E5; // "day"
            p[1] = L' ';
            FormatNumber(cal.hour, p + 2, 2, FALSE);
            wcscat(mDateText, L"\x6642\x767A\x8868"); // "o'clock announcement"
            FIT_DATE(sDateScaleXJP, sDateScaleYJP);
        } else {
            mDateText[0] = 0;
        }
        MinutesToCalendarTime(time + 24 * 60, &cal);
        mTomorrowDay = cal.mday - 1;
    }
}

void WeatherNormal::SetupDate() {
    if (gCurrentCity != NULL) {
        CityForecast* forecast;
        CitySummary* summary;
        CityNow* now = gCurrentCity->mNow;
        forecast = gCurrentCity->mForecast;
        summary = gCurrentCity->mSummary;
        u32 time;
        u16 nowWeather;
        u16 today;
        u16 tomorrow;
        OSCalendarTime cal;

        if (now != NULL) {
            ShortEntry* entry = now->mEntry;
            nowWeather = entry->mWeather;
            MinutesToCalendarTime(entry->mTime, &cal);
            mNowDay = cal.wday;
        }

        if (forecast != NULL) {
            mDateType = 0;
            ForecastEntry* entry = forecast->mEntry;
            time = entry->mTime;
            today = entry->mDays[0].mWeather;
            tomorrow = entry->mDays[1].mWeather;
            MinutesToCalendarTime(time, &cal);
            mForecastDay = cal.wday;
            MinutesToCalendarTime(time + 24 * 60, &cal);
            mForecastDay2 = cal.wday;
        } else if (summary != NULL) {
            mDateType = 1;
            SummaryEntry* entry = summary->mEntry;
            time = entry->mTime;
            today = entry->mDays[0].mWeather;
            tomorrow = entry->mDays[1].mWeather;
            MinutesToCalendarTime(time, &cal);
            mForecastDay = cal.wday;
            MinutesToCalendarTime(time + 24 * 60, &cal);
            mForecastDay2 = cal.wday;
        }

        WeatherInfo* info = gForecastData->FindWeatherInfo(nowWeather);
        if (info != NULL) {
            mNowIcon = info->mType->mIcon;
        }
        info = gForecastData->FindWeatherInfo(today);
        if (info != NULL) {
            mTodayIcon = info->mType->mIcon;
        }
        info = gForecastData->FindWeatherInfo(tomorrow);
        if (info != NULL) {
            mTomorrowIcon = info->mType->mIcon;
        }

        if (gForecastPage == 1) {
            if (now != NULL) {
                time = now->mEntry->mTime;
            } else {
                return;
            }
        } else if (forecast != NULL) {
            time = forecast->mEntry->mTime;
        } else if (summary != NULL) {
            time = summary->mEntry->mTime;
        } else {
            return;
        }

        gTextWriter.SetFont(*gSysFont);
        if (time != 0) {
            if (mFormatDate) {
                (this->*mFormatDate)(time);
            }
        } else {
            mDateText[0] = 0;
        }
    }
}

static f32 sDateScaleXUS = 0.6f;
static f32 sDateScaleYUS = 0.6f;

// "Updated 3:05 p.m., 06/21"
void WeatherNormal::FormatDateUS(u32 minutes) {
    OSCalendarTime cal;
    MinutesToCalendarTime(minutes, &cal);

    s32 hour = cal.hour;
    if (hour == 0) {
        hour = 12;
    } else if (hour > 12) {
        hour -= 12;
    }

    wcscpy(mDateText, gUpdatedPrefixes[gLanguage]);
    wchar_t* p = FormatNumber(hour, mDateText + wcslen(mDateText), 2, FALSE);
    *p = L':';
    FormatNumber(cal.min, p + 1, 2, TRUE);
    wcscat(mDateText, L" ");
    if (cal.hour < 12) {
        wcscat(mDateText, gAmText);
    } else {
        wcscat(mDateText, gPmText);
    }
    wcscat(mDateText, L", ");
    p = FormatNumber(cal.month + 1, mDateText + wcslen(mDateText), 2, TRUE);
    *p = L'/';
    FormatNumber(cal.mday, p + 1, 2, TRUE);
    FIT_DATE(sDateScaleXUS, sDateScaleYUS);
}

static f32 sDateScaleXEU = 0.6f;
static f32 sDateScaleYEU = 0.6f;

// "Last Updated: 21/06/2008 15:05"
void WeatherNormal::FormatDateEU(u32 minutes) {
    OSCalendarTime cal;
    MinutesToCalendarTime(minutes, &cal);
    s32 hour = cal.hour;

    wcscpy(mDateText, gLastUpdatedPrefixes[gLanguage]);
    wchar_t* p = FormatNumber(cal.mday, mDateText + wcslen(mDateText), 2, TRUE);
    *p = L'/';
    p = FormatNumber(cal.month + 1, p + 1, 2, TRUE);
    *p = L'/';
    p = FormatNumber(cal.year, p + 1, 4, FALSE);
    *p = L' ';
    p = FormatNumber(hour, p + 1, 2, TRUE);
    *p = L':';
    FormatNumber(cal.min, p + 1, 2, TRUE);
    FIT_DATE(sDateScaleXEU, sDateScaleYEU);
}

static f32 sDateScaleXDE = 0.6f;
static f32 sDateScaleYDE = 0.6f;

// "Stand: 21.06.2008 - 15:05"
void WeatherNormal::FormatDateDE(u32 minutes) {
    OSCalendarTime cal;
    MinutesToCalendarTime(minutes, &cal);
    s32 hour = cal.hour;

    wcscpy(mDateText, gLastUpdatedPrefixes[gLanguage]);
    wchar_t* p = FormatNumber(cal.mday, mDateText + wcslen(mDateText), 2, TRUE);
    *p = L'.';
    p = FormatNumber(cal.month + 1, p + 1, 2, TRUE);
    *p = L'.';
    p = FormatNumber(cal.year, p + 1, 4, FALSE);
    p[0] = L' ';
    p[1] = L'-';
    p[2] = L' ';
    p = FormatNumber(hour, p + 3, 2, TRUE);
    *p = L':';
    FormatNumber(cal.min, p + 1, 2, TRUE);
    FIT_DATE(sDateScaleXDE, sDateScaleYDE);
}

static f32 sDateScaleXCA = 0.6f;
static f32 sDateScaleYCA = 0.6f;

// "Le 06-21 \xE0 15:05" (Canadian French)
void WeatherNormal::FormatDateCA(u32 minutes) {
    OSCalendarTime cal;
    MinutesToCalendarTime(minutes, &cal);

    swprintf(mDateText, 0x80, L"%ls%02d-%02d %lc %02d:%02d", gUpdatedPrefixes[gLanguage], cal.month + 1, cal.mday,
             0xE0, cal.hour, cal.min);
    FIT_DATE(sDateScaleXCA, sDateScaleYCA);
}

static f32 sDateScaleXFR = 0.6f;
static f32 sDateScaleYFR = 0.6f;

// "Le 21/06 \xE0 15:05"
void WeatherNormal::FormatDateFR(u32 minutes) {
    OSCalendarTime cal;
    MinutesToCalendarTime(minutes, &cal);
    s32 hour = cal.hour;

    wcscpy(mDateText, gLastUpdatedPrefixes[gLanguage]);
    wchar_t* p = FormatNumber(cal.mday, mDateText + wcslen(mDateText), 2, TRUE);
    *p = L'/';
    p = FormatNumber(cal.month + 1, p + 1, 2, TRUE);
    p[0] = L' ';
    p[1] = 0xE0;
    p[2] = L' ';
    p = FormatNumber(hour, p + 3, 2, TRUE);
    *p = L':';
    FormatNumber(cal.min, p + 1, 2, TRUE);
    FIT_DATE(sDateScaleXFR, sDateScaleYFR);
}

static f32 sDateScaleXES = 0.6f;
static f32 sDateScaleYES = 0.6f;

// "21-06-2008 15:05"
void WeatherNormal::FormatDateES(u32 minutes) {
    OSCalendarTime cal;
    MinutesToCalendarTime(minutes, &cal);
    s32 hour = cal.hour;

    wchar_t* p = FormatNumber(cal.mday, mDateText, 2, TRUE);
    *p = L'-';
    p = FormatNumber(cal.month + 1, p + 1, 2, TRUE);
    *p = L'-';
    p = FormatNumber(cal.year, p + 1, 4, FALSE);
    *p = L' ';
    p = FormatNumber(hour, p + 1, 2, TRUE);
    *p = L':';
    FormatNumber(cal.min, p + 1, 2, TRUE);
    FIT_DATE(sDateScaleXES, sDateScaleYES);
}

static f32 sDateScaleXIT = 0.6f;
static f32 sDateScaleYIT = 0.6f;

// "Aggiornato il 21/06/2008 15:05"
void WeatherNormal::FormatDateIT(u32 minutes) {
    OSCalendarTime cal;
    MinutesToCalendarTime(minutes, &cal);
    s32 hour = cal.hour;

    wcscpy(mDateText, gLastUpdatedPrefixes[gLanguage]);
    wchar_t* p = FormatNumber(cal.mday, mDateText + wcslen(mDateText), 2, TRUE);
    *p = L'/';
    p = FormatNumber(cal.month + 1, p + 1, 2, TRUE);
    *p = L'/';
    p = FormatNumber(cal.year, p + 1, 4, FALSE);
    *p = L' ';
    p = FormatNumber(hour, p + 1, 2, TRUE);
    *p = L':';
    FormatNumber(cal.min, p + 1, 2, TRUE);
    FIT_DATE(sDateScaleXIT, sDateScaleYIT);
}

static f32 sDateScaleXNL = 0.6f;
static f32 sDateScaleYNL = 0.6f;

// "21-06-2008 (15:05 uur)"
void WeatherNormal::FormatDateNL(u32 minutes) {
    OSCalendarTime cal;
    MinutesToCalendarTime(minutes, &cal);
    s32 hour = cal.hour;

    wchar_t* p = FormatNumber(cal.mday, mDateText, 2, TRUE);
    *p = L'-';
    p = FormatNumber(cal.month + 1, p + 1, 2, TRUE);
    *p = L'-';
    p = FormatNumber(cal.year, p + 1, 4, FALSE);
    p[0] = L' ';
    p[1] = L'(';
    p = FormatNumber(hour, p + 2, 2, TRUE);
    *p = L':';
    FormatNumber(cal.min, p + 1, 2, TRUE);
    wcscat(mDateText, L" uur)");
    FIT_DATE(sDateScaleXNL, sDateScaleYNL);
}

void WeatherNormal::Draw() {
    u8 alpha = 255.0f * mAlpha;
    SetDefaultGXState();
    SetOrthoProjection();

    s32 baseAlpha;
    if (gShowAmbientSound) {
        baseAlpha = 255.0f * mAlpha;
    } else {
        baseAlpha = 128.0f * mAlpha;
    }
    if (baseAlpha != 0) {
        mBaseLayout->SetPaneAlpha(baseAlpha);
        mBaseLayout->Draw();
    }

    for (s32 i = 0; i < gForecastPageCount; i++) {
        mPages[i]->Draw();
    }

    if (mFlash) {
        s32 width = GetScreenWidth();
        f32 halfHeight = mAlpha * 228.0f;
        f32 halfWidth = mAlpha * (0.5f * width);
        nw4r::ut::Rect rect(mFlashX - halfWidth, mFlashY - halfHeight, mFlashX + halfWidth, mFlashY + halfHeight);
        SetDefaultGXState();
        SetOrthoProjection();
        GXColor color;
        color.r = 255;
        color.g = 255;
        color.b = 255;
        color.a = mFlashAlpha;
        DrawRect((Rect*)&rect, &color);
    }

    if (mActive) {
        mLayout->Draw();
    }

    if (mZoomed) {
        DrawTimes();
    }

    if (mShowZoom) {
        SetDefaultGXState();
        SetOrthoProjection();
        mZoomColors[0].a = mZoomAlpha;
        mZoomColors[1].a = mZoomAlpha;
        mZoomColors[2].a = mZoomAlpha;
        mZoomColors[3].a = mZoomAlpha;
        DrawGradientRect((Rect*)&mZoomRect, (GXColor*)mZoomColors);
    }

    if (alpha != 0) {
        mBeltLayout->SetPaneAlpha(alpha);
        mBeltLayout->Draw();
    }

    SetDefaultGXState();
    SetOrthoProjection();
    gTextWriter.SetFont(*gSysFont);
    gTextWriter.SetDrawFlag(0x122);
    gTextWriter.SetupGX();

    CityNameLine* line = mNames;
    f32 lineHeight = mBelt[1].mRect.bottom - mBelt[1].mRect.top;
    f32 y = mNameScroll + (mCityRect.top + 0.5f * (mCityRect.bottom - mCityRect.top));
    SetScaledScissor(0, mBelt[1].mRect.top, GetScreenWidth(), lineHeight);
    switch (mDateType) {
    case 0:
        for (s32 i = 0; i < mNumNames; i++, line++) {
            f32 offset = 1.0f;
            gTextWriter.SetTextColor(nw4r::ut::Color(32, 32, 32, alpha));
            gTextWriter.SetScale(line->mScale, mNameScaleY);
            gTextWriter.SetCharSpace(line->mScale * gUnkSceneFloat);
            gTextWriter.SetCursor(offset + mCityRect.right, offset + y);
            gTextWriter.Print(line->mText);
            gTextWriter.SetTextColor(nw4r::ut::Color(255, 255, 255, alpha));
            gTextWriter.SetCursor(mCityRect.right, y);
            gTextWriter.Print(line->mText);
            y += lineHeight;
        }
        break;
    case 1:
        for (s32 i = 0; i < mNumNames; i++, line++) {
            f32 offset = 1.0f;
            gTextWriter.SetTextColor(nw4r::ut::Color(32, 32, 32, alpha));
            gTextWriter.SetScale(line->mScale, mNameScaleY);
            gTextWriter.SetCharSpace(line->mScale * gUnkSceneFloat);
            gTextWriter.SetCursor(offset + mCityRect.right, offset + y);
            gTextWriter.Print(line->mText);
            gTextWriter.SetTextColor(nw4r::ut::Color(255, 255, 255, alpha));
            gTextWriter.SetCursor(mCityRect.right, y);
            gTextWriter.Print(line->mText);
            y += lineHeight;
        }
        break;
    }
    SetScaledScissor(0, 0, GetScreenWidth(), 456);

    if (gLanguage == 0) {
        gTextWriter.SetDrawFlag(0x111);
        gTextWriter.SetTextColor(nw4r::ut::Color(255, 255, 255, alpha));
    } else {
        u8 dateAlpha = mDateAlpha * mAlpha;
        gTextWriter.SetDrawFlag(0x122);
        gTextWriter.SetTextColor(nw4r::ut::Color(255, 255, 255, dateAlpha));
    }
    gTextWriter.SetupGX();
    gTextWriter.SetScale(mDateScaleX, mDateScaleY);
    gTextWriter.SetCharSpace(mDateScaleX * gUnkSceneFloat);
    gTextWriter.SetCursor(mDateX, mDateY);
    gTextWriter.Print(mDateText);
}

void WeatherNormal::DrawTimes() {
    if (gCurrentCity != NULL) {
        CityForecast* forecast = gCurrentCity->mForecast;
        CitySummary* summary = gCurrentCity->mSummary;
        DayForecast* day;
        s32 hour;
        if (forecast != NULL) {
            day = forecast->mEntry->mDays;
            hour = 0;
        } else if (summary != NULL) {
            day = summary->mEntry->mDays;
            hour = 0;
        } else {
            return;
        }

        mTimeLayout->SetPaneAlpha(255.0f * mTimesAlpha);
        mTimeLayout->Draw();
        if (mDrawTimes) {
            (this->*mDrawTimes)(day, hour);
        }
    }
}

// Draws the four 6-hour weather icons, or "--" where there is no forecast
#define DRAW_ICONS()                                                                                     \
    box = mBoxes;                                                                                        \
    for (i = 0; i < 4; i++, box++) {                                                                     \
        u32 code = d->mWeatherParts[i];                                                                  \
        WeatherInfo* info = gForecastData->FindWeatherInfo(code);                                        \
        if (d->mWeatherParts[i] != 0xFFFF && info != NULL) {                                             \
            u16 icon = info->mType->mIcon;                                                               \
            DrawWeatherIcon(icon, (Vec2*)&box->mX, box->mScaleX, alpha);                                 \
        } else {                                                                                         \
            wcscpy(sTextBuf, L"--");                                                                     \
            box->mColor.a = alpha;                                                                       \
            box->mShadowColor.a = alpha;                                                                 \
            SetDefaultGXState();                                                                         \
            SetOrthoProjection();                                                                        \
            DrawTempCentered(sTextBuf, (Vec2*)&box->mX, box->mScaleX, box->mScaleY, &box->mColor,        \
                             &box->mShadowColor);                                                        \
        }                                                                                                \
    }

#define SETUP_TIME_TEXT(flag)                                                                                \
    SetDefaultGXState();                                                                                     \
    SetOrthoProjection();                                                                                    \
    gTextWriter.SetFont(*gSysFont);                                                                          \
    gTextWriter.SetDrawFlag(flag);                                                                           \
    gTextWriter.SetupGX();                                                                                   \
    gTextWriter.SetCharSpace(0.0f)

#define PRINT_TIME(box, alpha, x)                                                                            \
    (box)->mColor.a = alpha;                                                                                 \
    gTextWriter.SetScale((box)->mScaleX, (box)->mScaleY);                                                    \
    gTextWriter.SetTextColor((box)->mColor);                                                                 \
    gTextWriter.SetCursor(x, (box)->mY);                                                                     \
    gTextWriter.Print(sTextBuf)

// "00-06時"
void WeatherNormal::DrawTimesJP(DayForecast* day, s32 hour) {
    DayForecast* d;
    switch (gForecastPage) {
    case 1:
        d = day;
        break;
    case 2:
        d = day + 1;
        break;
    default:
        return;
    }

    f32 small = 0.6f;
    s32 alpha = 255.0f * mTimesAlpha;
    TextBox* box = mBoxes;
    for (s32 i = 0; i < 4; i++, box++) {
        u32 code = d->mWeatherParts[i];
        WeatherInfo* info = gForecastData->FindWeatherInfo(code);
        if (d->mWeatherParts[i] != 0xFFFF && info != NULL) {
            u16 icon = info->mType->mIcon;
            f32 scale;
            if ((icon & 0x7FFF) < 100) {
                scale = box->mScaleX;
            } else {
                scale = small * box->mScaleX;
            }
            DrawWeatherIcon(icon, (Vec2*)&box->mX, scale, alpha);
        } else {
            wcscpy(sTextBuf, L"--");
            box->mColor.a = alpha;
            box->mShadowColor.a = alpha;
            SetDefaultGXState();
            SetOrthoProjection();
            DrawDateCentered(sTextBuf, (Vec2*)&box->mX, box->mScaleX, box->mScaleY, &box->mColor,
                             &box->mShadowColor);
        }
    }

    SETUP_TIME_TEXT(0x111);
    s32 h = hour;
    {
        TextBox* box = &mBoxes[4];
        for (s32 i = 0; i < 4; i++, box++) {
            wchar_t* p = FormatNumber(h, sTextBuf, 2, FALSE);
            *p = L'-';
            h += 6;
            WrapHourTo24(&h);
            FormatNumber(h, p + 1, 2, FALSE);
            wcscat(sTextBuf, L"\x6642"); // "o'clock"
            PRINT_TIME(box, alpha, box->mX);
        }
    }
}

// The 12-hour clock labels
static const wchar_t* sHours12[12] = {
    L"12", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9", L"10", L"11",
};

// "12:00 a.m.\n6:00 a.m."
void WeatherNormal::DrawTimesUS(DayForecast* day, s32 hour) {
    DayForecast* d;
    s32 alpha;
    TextBox* box;
    s32 i;
    switch (gForecastPage) {
    case 2:
        d = day;
        break;
    case 3:
        d = day + 1;
        break;
    default:
        return;
    }

    alpha = 255.0f * mTimesAlpha;
    DRAW_ICONS();

    SETUP_TIME_TEXT(0x122);
    box = &mBoxes[4];
    f32 half = 0.5f;
    for (i = 0; i < 4; i++, box++) {
        wcscpy(sTextBuf, sHours12[hour % 12]);
        wcscat(sTextBuf, L":00 ");
        if (hour < 12) {
            wcscat(sTextBuf, gAmText);
        } else {
            wcscat(sTextBuf, gPmText);
        }
        wcscat(sTextBuf, L"\n");
        hour += 6;
        hour %= 24;
        wcscat(sTextBuf, sHours12[hour % 12]);
        wcscat(sTextBuf, L":00 ");
        if (hour < 12) {
            wcscat(sTextBuf, gAmText);
        } else {
            wcscat(sTextBuf, gPmText);
        }
        f32 offset = half * gTextWriter.CalcStringWidth(sTextBuf);
        PRINT_TIME(box, alpha, box->mX + offset);
    }
}

// "00:00\n06:00"
void WeatherNormal::DrawTimesEU(DayForecast* day, s32 hour) {
    DayForecast* d;
    TextBox* box;
    s32 i;
    s32 h;
    s32 alpha;
    switch (gForecastPage) {
    case 2:
        d = day;
        break;
    case 3:
        d = day + 1;
        break;
    default:
        return;
    }

    alpha = 255.0f * mTimesAlpha;
    DRAW_ICONS();

    SETUP_TIME_TEXT(0x122);
    h = hour;
    box = &mBoxes[4];
    f32 half = 0.5f;
    for (i = 0; i < 4; i++, box++) {
        wchar_t start[4];
        wchar_t end[4];
        FormatNumber(h, start, 2, TRUE);
        wcscpy(sTextBuf, start);
        wcscat(sTextBuf, L":00 ");
        wcscat(sTextBuf, L"\n");
        h += 6;
        WrapHourTo24(&h);
        FormatNumber(h, end, 2, TRUE);
        wcscat(sTextBuf, end);
        wcscat(sTextBuf, L":00 ");
        f32 offset = half * gTextWriter.CalcStringWidth(sTextBuf);
        PRINT_TIME(box, alpha, box->mX + offset);
    }
}

// "00:00-\n06:00"
void WeatherNormal::DrawTimesDE(DayForecast* day, s32 hour) {
    DayForecast* d;
    TextBox* box;
    s32 i;
    s32 h;
    s32 alpha;
    switch (gForecastPage) {
    case 2:
        d = day;
        break;
    case 3:
        d = day + 1;
        break;
    default:
        return;
    }

    alpha = 255.0f * mTimesAlpha;
    DRAW_ICONS();

    SETUP_TIME_TEXT(0x100);
    h = hour;
    box = &mBoxes[4];
    f32 half = 0.5f;
    for (i = 0; i < 4; i++, box++) {
        wchar_t start[4];
        wchar_t end[4];
        FormatNumber(h, start, 2, TRUE);
        wcscpy(sTextBuf, start);
        wcscat(sTextBuf, L":00-");
        wcscat(sTextBuf, L"\n");
        h += 6;
        WrapHourTo24(&h);
        FormatNumber(h, end, 2, TRUE);
        wcscat(sTextBuf, end);
        wcscat(sTextBuf, L":00");
        f32 offset = half * gTextWriter.CalcStringWidth(sTextBuf);
        PRINT_TIME(box, alpha, box->mX - offset);
    }
}

// The 24-hour clock labels
static const wchar_t* sHours24[24] = {
    L"00", L"01", L"02", L"03", L"04", L"05", L"06", L"07", L"08", L"09", L"10", L"11",
    L"12", L"13", L"14", L"15", L"16", L"17", L"18", L"19", L"20", L"21", L"22", L"23",
};

// "De 00:00\n\xE0 06:00"
void WeatherNormal::DrawTimesFR(DayForecast* day, s32 hour) {
    DayForecast* d;
    s32 alpha;
    TextBox* box;
    s32 i;
    switch (gForecastPage) {
    case 2:
        d = day;
        break;
    case 3:
        d = day + 1;
        break;
    default:
        return;
    }

    alpha = 255.0f * mTimesAlpha;
    DRAW_ICONS();

    SETUP_TIME_TEXT(0x122);
    s32 h = hour;
    box = &mBoxes[4];
    f32 half = 0.5f;
    for (i = 0; i < 4; i++, box++) {
        wcscpy(sTextBuf, L"De ");
        wcscat(sTextBuf, sHours24[h]);
        wcscat(sTextBuf, L":00");
        wcscat(sTextBuf, L"\n");
        h += 6;
        WrapHour(&h);
        wchar_t* p = &sTextBuf[wcslen(sTextBuf)];
        p[0] = 0xE0;
        p[1] = L' ';
        p[2] = 0;
        wcscat(sTextBuf, sHours24[h]);
        wcscat(sTextBuf, L":00");
        f32 offset = half * gTextWriter.CalcStringWidth(sTextBuf);
        PRINT_TIME(box, alpha, box->mX + offset);
    }
}

// "De 00:00\na 06:00"
void WeatherNormal::DrawTimesES(DayForecast* day, s32 hour) {
    DayForecast* d;
    s32 alpha;
    TextBox* box;
    s32 i;
    switch (gForecastPage) {
    case 2:
        d = day;
        break;
    case 3:
        d = day + 1;
        break;
    default:
        return;
    }

    alpha = 255.0f * mTimesAlpha;
    DRAW_ICONS();

    SETUP_TIME_TEXT(0x122);
    s32 h = hour;
    box = &mBoxes[4];
    f32 half = 0.5f;
    for (i = 0; i < 4; i++, box++) {
        wchar_t start[4];
        wchar_t end[4];
        FormatNumber(h, start, 2, TRUE);
        wcscpy(sTextBuf, L"De ");
        wcscat(sTextBuf, start);
        wcscat(sTextBuf, L":00");
        wcscat(sTextBuf, L"\n");
        h += 6;
        WrapHour(&h);
        FormatNumber(h, end, 2, TRUE);
        wcscat(sTextBuf, L"a ");
        wcscat(sTextBuf, end);
        wcscat(sTextBuf, L":00");
        f32 offset = half * gTextWriter.CalcStringWidth(sTextBuf);
        PRINT_TIME(box, alpha, box->mX + offset);
    }
}

// "00:00\n06:00"
void WeatherNormal::DrawTimesIT(DayForecast* day, s32 hour) {
    DayForecast* d;
    s32 alpha;
    TextBox* box;
    s32 i;
    switch (gForecastPage) {
    case 2:
        d = day;
        break;
    case 3:
        d = day + 1;
        break;
    default:
        return;
    }

    alpha = 255.0f * mTimesAlpha;
    DRAW_ICONS();

    SETUP_TIME_TEXT(0x122);
    s32 h = hour;
    box = &mBoxes[4];
    f32 half = 0.5f;
    for (i = 0; i < 4; i++, box++) {
        wchar_t start[4];
        wchar_t end[4];
        FormatNumber(h, start, 2, TRUE);
        wcscpy(sTextBuf, start);
        wcscat(sTextBuf, L":00");
        wcscat(sTextBuf, L"\n");
        h += 6;
        WrapHour(&h);
        FormatNumber(h, end, 2, TRUE);
        wcscat(sTextBuf, end);
        wcscat(sTextBuf, L":00");
        f32 offset = half * gTextWriter.CalcStringWidth(sTextBuf);
        PRINT_TIME(box, alpha, box->mX + offset);
    }
}

// "00:00 -\n06:00 uur"
void WeatherNormal::DrawTimesNL(DayForecast* day, s32 hour) {
    DayForecast* d;
    s32 alpha;
    TextBox* box;
    s32 i;
    switch (gForecastPage) {
    case 2:
        d = day;
        break;
    case 3:
        d = day + 1;
        break;
    default:
        return;
    }

    alpha = 255.0f * mTimesAlpha;
    DRAW_ICONS();

    SETUP_TIME_TEXT(0x100);
    s32 h = hour;
    box = &mBoxes[4];
    f32 half = 0.5f;
    for (i = 0; i < 4; i++, box++) {
        wchar_t start[4];
        wchar_t end[4];
        FormatNumber(h, start, 2, TRUE);
        wcscpy(sTextBuf, start);
        wcscat(sTextBuf, L":00 -");
        wcscat(sTextBuf, L"\n");
        h += 6;
        WrapHour(&h);
        FormatNumber(h, end, 2, TRUE);
        wcscat(sTextBuf, end);
        wcscat(sTextBuf, L":00 uur");
        f32 offset = half * gTextWriter.CalcStringWidth(sTextBuf);
        PRINT_TIME(box, alpha, box->mX - offset);
    }
}

#define SET_ARROWS(up, down)                                                                                 \
    if ((up) < 0) {                                                                                          \
        mUpButton->Lock();                                                                                   \
    } else {                                                                                                 \
        mUpButton->Unlock();                                                                                 \
        mUpButton->SetState(up);                                                                             \
    }                                                                                                        \
    if ((down) < 0) {                                                                                        \
        mDownButton->Lock();                                                                                 \
    } else {                                                                                                 \
        mDownButton->Unlock();                                                                               \
        mDownButton->SetState(down);                                                                         \
    }

void WeatherNormal::UpdateArrows() {
    if (gLanguage == 0) {
        switch (gForecastPage) {
        case 0:
            SET_ARROWS(-1, 1);
            break;
        case 1:
            SET_ARROWS(0, 2);
            break;
        case 2:
            SET_ARROWS(2, 3);
            break;
        case 3:
            SET_ARROWS(3, -1);
            break;
        }
    } else {
        switch (gForecastPage) {
        case 0:
            SET_ARROWS(-1, 0);
            break;
        case 1:
            SET_ARROWS(0, 1);
            break;
        case 2:
            SET_ARROWS(1, 2);
            break;
        case 3:
            SET_ARROWS(2, 3);
            break;
        case 4:
            SET_ARROWS(3, -1);
            break;
        }
    }
}

#define FADE_TOWARDS(value, target, step)                                                                    \
    if ((value) < (target)) {                                                                                \
        (value) += step;                                                                                     \
        if ((value) > (target)) {                                                                            \
            (value) = (target);                                                                              \
        }                                                                                                    \
    } else if ((value) > (target)) {                                                                         \
        (value) -= step;                                                                                     \
        if ((value) < (target)) {                                                                            \
            (value) = (target);                                                                              \
        }                                                                                                    \
    }

void WeatherNormal::UpdateBelt() {
    FADE_TOWARDS(mBeltAlpha, mBeltTarget, 16);

    if (unk818 > 0) {
        unk818 -= 16;
        if (unk818 < 0) {
            unk818 = 0;
        }
    }

    FADE_TOWARDS(mDateAlpha, mDateTarget, 16);

    LayoutButton* belt = mBeltLayout->FindButton("belt");
    if (belt != NULL) {
        belt->SetChildAlpha("beltT", mBeltAlpha);
    }
}

BOOL WeatherNormal::ChangeState(StateFunc state, s32 arg) {
    if (mState) {
        mPhase = -1;
        (this->*mState)(arg);
    }
    mState = state;
    mPhase = 0;
    if (mState) {
        return (this->*mState)(arg);
    }
    return TRUE;
}

#define LAYOUT_PAGES()                                                                                       \
    {                                                                                                        \
        f32 y = mPageY;                                                                                      \
        for (s32 i = 0; i < gForecastPageCount; i++) {                                                       \
            mPageVisible[i] = TRUE;                                                                          \
            mPagePos[i].x = mPageX;                                                                          \
            mPagePos[i].y = y;                                                                               \
            y -= 456.0f;                                                                                     \
        }                                                                                                    \
    }

#define SET_BELT_STATE(state)                                                                                \
    {                                                                                                        \
        s32 beltState = state;                                                                               \
        LayoutButton* belt = mBeltLayout->FindButton("belt");                                                \
        if (belt != NULL) {                                                                                  \
            belt->SetState(beltState);                                                                       \
        }                                                                                                    \
    }

BOOL WeatherNormal::StateScroll(s32 arg) {
    UpdateWeatherSound();
    switch (mPhase) {
    case 0:
        mPhase++;
        mMoveX = mPages[gForecastPage]->mSize.y;
        LAYOUT_PAGES();
        mBeltTarget = 0;
        mDateTarget = 0;
        break;
    case -1:
        break;
    default: {
        f32 diff = SmoothApproach(&mPageY, mMoveX, 0.1125f, 100.0f, 1.0f);
        if (IsNearZero(diff)) {
            CHANGE_STATE(&WeatherNormal::StateNormal);
            return TRUE;
        }

        if (mBeltAlpha == 0) {
            SET_BELT_STATE(mPages[gForecastPage]->mType);
            mBeltTarget = 255;
            mDateTarget = 255;
            SetCity();
        }

        LAYOUT_PAGES();
        UpdateArrows();

        if (mDownPressed) {
            if (gForecastPage < gForecastPageCount - 1) {
                gForecastPage++;
                PlaySE(41);
                mPhase = 0;
                return TRUE;
            }
        } else if (mUpPressed && gForecastPage != 0) {
            gForecastPage--;
            PlaySE(41);
            mPhase = 0;
            return TRUE;
        }
        break;
    }
    }
    return TRUE;
}

#define START_ZOOM_ANIM()                                                                                    \
    {                                                                                                        \
        Vec2F iconPos = ((WeatherBaseDay*)page)->GetIconPos();                                               \
        mZoomX = iconPos.x;                                                                                  \
        mZoomY = iconPos.y;                                                                                  \
        mMoveX = -mZoomX;                                                                                    \
        mMoveY = -mZoomY;                                                                                    \
    }

#define UPDATE_ZOOM_ANIM()                                                                                   \
    {                                                                                                        \
        f32 t = EaseCos(mAnimTimer);                                                                         \
        mTimesAlpha = t;                                                                                     \
        mZoomAlpha = 40.0f * t;                                                                              \
        mZoomRect.left = mZoomX + mMoveX * t;                                                                \
        mZoomRect.top = mZoomY + mMoveY * t;                                                                 \
        mZoomRect.right = mZoomX + mMoveX2 * t;                                                              \
        mZoomRect.bottom = mZoomY + mMoveY2 * t;                                                             \
    }

BOOL WeatherNormal::StateNormal(s32 arg) {
    UpdateWeatherSound();
    switch (mPhase) {
    case 0: {
        mPhase++;
        WeatherBase* page = mPages[gForecastPage];
        mPageY = page->mSize.y;
        mBeltState = page->mType;
        SET_BELT_STATE(page->mType);
        LAYOUT_PAGES();
        mBeltTarget = 255;
        mDateTarget = 255;
        if (mSetupBelt) {
            (this->*mSetupBelt)();
        }

        if (IsScrollState(&WeatherNormal::ScrollNames)) {
            mScrollPhase = 2;
            mNameIndex = 0;
            mScrollTimer = 240;
        } else {
            CHANGE_SCROLL(&WeatherNormal::ScrollNames);
        }

        SetCity();
        mBeltPhase = 0;
        if (mUpdateBeltText) {
            (this->*mUpdateBeltText)();
        }
        SET_BELT_STATE(mBeltState);
        UpdateArrows();
        break;
    }
    case -1:
        break;
    default:
        switch (mPhase) {
        case 1:
            if (IsDetailPressed()) {
                WeatherBase* page = mPages[gForecastPage];
                mPhase++;
                mAnimTimer = 0;
                mZoomed = TRUE;
                mShowZoom = TRUE;
                mTimesAlpha = EaseCos(0);
                mZoomAlpha = 40.0f * mTimesAlpha;
                START_ZOOM_ANIM();
                s32 width = GetScreenWidth();
                mZoomRect.right = mZoomX;
                mZoomRect.left = mZoomX;
                mMoveY2 = 456.0f - mZoomY;
                mMoveX2 = width - mZoomX;
                mZoomRect.bottom = mZoomY;
                mZoomRect.top = mZoomY;
                PlaySE(42);
            } else if (mDownPressed) {
                if (gForecastPage < gForecastPageCount - 1) {
                    gForecastPage++;
                    PlaySE(41);
                    CHANGE_STATE(&WeatherNormal::StateScroll);
                    return TRUE;
                }
            } else if (mUpPressed) {
                if (gForecastPage != 0) {
                    gForecastPage--;
                    PlaySE(41);
                    CHANGE_STATE(&WeatherNormal::StateScroll);
                    return TRUE;
                }
            } else if (!gShowAmbientSound && IsBackPressed()) {
                PlaySE(38);
                gSettingResult = 3;
                CHANGE_STATE(&WeatherNormal::StateToAround);
                return TRUE;
            }
            break;
        case 2:
            mAnimTimer += 0x800;
            if (mAnimTimer >= 0x8000) {
                mAnimTimer = 0x8000;
                mPhase++;
                mShowZoom = FALSE;
            } else {
                UPDATE_ZOOM_ANIM();
            }
            break;
        case 3:
            if (gTrigAll & WPAD_BUTTON_A) {
                WeatherBase* page = mPages[gForecastPage];
                mPhase++;
                mAnimTimer = 0x8000;
                mShowZoom = TRUE;
                mTimesAlpha = EaseCos(0x8000);
                mZoomAlpha = 40.0f * mTimesAlpha;
                START_ZOOM_ANIM();
                mZoomRect.left = 0.0f;
                mZoomRect.top = 0.0f;
                mMoveX2 = GetScreenWidth() - mZoomX;
                mMoveY2 = 456.0f - mZoomY;
                mZoomRect.bottom = 456.0f;
                mZoomRect.right = GetScreenWidth();
                PlaySE(43);
            }
            break;
        case 4:
        default:
            mAnimTimer -= 0x800;
            if (mAnimTimer <= 0) {
                mPhase = 1;
                mAnimTimer = 0;
                mShowZoom = FALSE;
                mZoomed = FALSE;
            } else {
                UPDATE_ZOOM_ANIM();
            }
            break;
        }
        UpdateArrows();
        if (mUpdateBeltText) {
            (this->*mUpdateBeltText)();
        }
        SET_BELT_STATE(mBeltState);
        break;
    }
    return TRUE;
}

void WeatherNormal::UpdateBeltTextJP() {
    switch (gForecastPage) {
    case 0: {
        WeatherBase* page = mPages[gForecastPage];
        if (page->IsEmpty()) {
            mBeltState = page->mType;
            return;
        }
        CycleBelt(((WeatherOther*)page)->mDay == 0 ? mTodayDay + 5 : mTomorrowDay + 5);
        return;
    }
    case 1: {
        WeatherBase* page = mPages[gForecastPage];
        if (page->IsEmpty()) {
            mBeltState = page->mType;
            return;
        }
        CycleBelt(mTodayDay + 5);
        return;
    }
    case 2: {
        WeatherBase* page = mPages[gForecastPage];
        if (page->IsEmpty()) {
            mBeltState = page->mType;
            return;
        }
        CycleBelt(mTomorrowDay + 5);
        return;
    }
    default:
        mBeltState = mPages[gForecastPage]->mType;
        return;
    }
}

void WeatherNormal::UpdateBeltText() {
    switch (gForecastPage) {
    case 0: {
        WeatherBase* page = mPages[gForecastPage];
        if (page->IsEmpty()) {
            mBeltState = page->mType;
            return;
        }
        CycleBelt(((WeatherOther*)page)->mDay == 0 ? mForecastDay + 5 : mForecastDay2 + 5);
        return;
    }
    case 1: {
        WeatherBase* page = mPages[gForecastPage];
        if (page->IsEmpty()) {
            mBeltState = page->mType;
            return;
        }
        CycleBelt(mNowDay + 5);
        return;
    }
    case 2: {
        WeatherBase* page = mPages[gForecastPage];
        if (page->IsEmpty()) {
            mBeltState = page->mType;
            return;
        }
        CycleBelt(mForecastDay + 5);
        return;
    }
    case 3: {
        WeatherBase* page = mPages[gForecastPage];
        if (page->IsEmpty()) {
            mBeltState = page->mType;
            return;
        }
        CycleBelt(mForecastDay2 + 5);
        return;
    }
    default:
        mBeltState = mPages[gForecastPage]->mType;
        return;
    }
}

// Alternates the belt between the page title and the day of the week
void WeatherNormal::CycleBelt(s32 state) {
    WeatherBase* page = mPages[gForecastPage];
    switch (mBeltPhase) {
    case 0:
        mBeltPhase++;
        mBeltTimer = 240;
        mBeltState = page->mType;
        break;
    case -1:
        break;
    default:
        if (mBeltTimer != 0) {
            mBeltTimer--;
        }
        switch (mBeltPhase) {
        case 1:
            if (mBeltTimer == 0) {
                mBeltPhase++;
                mBeltTarget = 0;
            }
            break;
        case 2:
            if (mBeltAlpha == 0) {
                mBeltPhase++;
                mBeltTimer = 240;
                mBeltState = state;
                mBeltTarget = 255;
            }
            break;
        case 3:
            if (mBeltTimer == 0) {
                mBeltPhase++;
                mBeltTarget = 0;
            }
            break;
        case 4:
        default:
            if (mBeltAlpha == 0) {
                mBeltPhase = 1;
                mBeltTimer = 240;
                mBeltState = page->mType;
                mBeltTarget = 255;
            }
            break;
        }
        break;
    }
}

void WeatherNormal::UpdateWeatherSound() {
    u16 icon;
    if (mZoomed || !mSoundEnabled) {
        return;
    }

    if (gLanguage == 0) {
        switch (gForecastPage) {
        case 1:
            icon = mTodayIcon;
            break;
        case 2:
            icon = mTomorrowIcon;
            break;
        default:
            return;
        }
    } else {
        switch (gForecastPage) {
        case 1:
            icon = mNowIcon;
            break;
        case 2:
            icon = mTodayIcon;
            break;
        case 3:
            icon = mTomorrowIcon;
            break;
        default:
            return;
        }
    }
    RequestWeatherSounds(icon, 0.7f);
}

BOOL WeatherNormal::IsDetailPressed() {
    if (mPages[gForecastPage]->IsEmpty()) {
        return FALSE;
    }

    if (gLanguage == 0) {
        if (gForecastPage >= 3 || gForecastPage < 1) {
            return FALSE;
        }
    } else if (gForecastPage >= 4 || gForecastPage < 2) {
        return FALSE;
    }

    s32 chan = mPages[gForecastPage]->unk98;
    if (chan >= 0 && (gTrig[chan] & WPAD_BUTTON_A)) {
        return TRUE;
    }
    return FALSE;
}

static f32 sAroundPageX = 0.0f;
static f32 sAroundUnk734 = 0.0f;
static f32 sAroundUnk738 = 0.0f;
static f32 sAroundUnk73C = 0.0f;

BOOL WeatherNormal::StateToAround(s32 arg) {
    f32 halfWidth = 0.5f * GetScreenWidth();
    f32 halfHeight = 228.0f;

    switch (mPhase) {
    case 0:
        if (gSimpleGlobe != NULL) {
            mPhase++;
            mPageX = sAroundPageX;
            mPageY = -456.0f;
            unk734 = sAroundUnk734;
            unk738 = sAroundUnk738;
            unk73C = sAroundUnk73C;
            mAlpha = 1.0f;
            mMoveX2 = halfWidth + mPagePos[gForecastPage].x;
            mMoveY2 = halfHeight + mPagePos[gForecastPage].y;
            mMoveX = gCityPos.x - mMoveX2;
            mAnimTimer = 0;
            mMoveY = gCityPos.y - mMoveY2;
            for (s32 i = 0; i < gForecastPageCount; i++) {
                mPageVisible[i] = i == gForecastPage;
            }
            mFlashX = mMoveX2;
            mFlash = TRUE;
            mFlashY = mMoveY2;
            mFlashAlpha = 40.0f * (1.0f - mAlpha);
            CHANGE_SCROLL(&WeatherNormal::StopScroll);
        }
        break;
    case -1:
        break;
    default:
        mAnimTimer += 0x800;
        if (mAnimTimer > 0x8000) {
            mFlash = FALSE;
            mAlpha = 0.0f;
            CHANGE_STATE(&WeatherNormal::StateOpenAround);
            return TRUE;
        }
        f32 t = 0.5f - 0.5f * nw4r::math::CosIdx(mAnimTimer);
        mAlpha = 1.0f - t;
        mFlashY = mMoveY2 + mMoveY * t;
        mFlashX = mMoveX2 + mMoveX * t;
        mFlashAlpha = 40.0f * (1.0f - mAlpha);
        break;
    }
    return TRUE;
}

BOOL WeatherNormal::StateAroundWait(s32 arg) {
    switch (mPhase) {
    case 0:
        mPhase++;
        CHANGE_SCROLL(&WeatherNormal::StopScroll);
        break;
    case -1:
        break;
    }
    return TRUE;
}

BOOL WeatherNormal::StateOpenAround(s32 arg) {
    switch (mPhase) {
    case 0:
        mPhase++;
        mAlpha = 0.0f;
        gShowAmbientSound = FALSE;
        for (s32 i = 0; i < gForecastPageCount; i++) {
            mPageVisible[i] = FALSE;
        }
        break;
    case -1:
        break;
    }
    return TRUE;
}

BOOL WeatherNormal::StateCloseAround(s32 arg) {
    switch (mPhase) {
    case -1:
        mUpButton->mToggle = FALSE;
        mDownButton->mToggle = FALSE;
        break;
    case 0: {
        mLayout->Reset();
        mUpButton->mToggle = TRUE;
        mDownButton->mToggle = TRUE;
        f32 centerX = 0.5f * GetScreenWidth();
        mPhase++;
        f32 centerY = 228.0f;
        WeatherBase* page = mPages[gForecastPage];
        mPageY = page->mSize.y;
        SET_BELT_STATE(page->mType);
        LAYOUT_PAGES();
        mMoveX2 = gCityPos.x;
        mMoveY2 = gCityPos.y;
        mMoveX = centerX - mMoveX2;
        mMoveY = centerY - mMoveY2;
        mAlpha = 0.0f;
        mAnimTimer = 0;
        for (s32 i = 0; i < gForecastPageCount; i++) {
            mPageVisible[i] = i == gForecastPage;
        }
        mFlashX = mMoveX2;
        mFlash = TRUE;
        mFlashY = mMoveY2;
        mFlashAlpha = 40.0f * (1.0f - mAlpha);
        UpdateArrows();
        break;
    }
    default:
        mAnimTimer += 0x800;
        if (mAnimTimer > 0x8000) {
            mFlash = FALSE;
            mAlpha = 1.0f;
            CHANGE_STATE(&WeatherNormal::StateNormal);
            return TRUE;
        }
        f32 t = 0.5f - 0.5f * nw4r::math::CosIdx(mAnimTimer);
        mAlpha = t;
        mFlashY = mMoveY2 + mMoveY * t;
        mFlashX = mMoveX2 + mMoveX * t;
        mFlashAlpha = 40.0f * (1.0f - t);
        UpdateArrows();
        break;
    }
    return TRUE;
}

void WeatherNormal::UpdateBeltHover() {
    mBelt[0].mHovered = FALSE;
    mBelt[1].mHovered = FALSE;
    for (s32 i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        if (gKPADLatest[i] >= 0) {
            f32 x = gCursorX[i];
            f32 y = gCursorY[i];
            for (s32 j = 0; j < 2; j++) {
                BeltText* belt = &mBelt[j];
                if (x > belt->mRect.left && x < belt->mRect.right && y > belt->mRect.top && y < belt->mRect.bottom) {
                    belt->mHovered = TRUE;
                }
            }
        }
    }
}

#define SETUP_BELT(lastPage, tomorrowPage)                                                                   \
    BeltText* belt = mBelt;                                                                                  \
    if (gForecastPage < lastPage && gForecastPage >= 1 && gCurrentCity != NULL) {                            \
        CityForecast* forecast = gCurrentCity->mForecast;                                                    \
        CitySummary* summary = gCurrentCity->mSummary;                                                       \
        u32 time;                                                                                            \
        if (forecast != NULL) {                                                                              \
            time = forecast->mEntry->mTime;                                                                  \
        } else if (summary != NULL) {                                                                        \
            time = summary->mEntry->mTime;                                                                   \
        } else {                                                                                             \
            return;                                                                                          \
        }                                                                                                    \
        if (gForecastPage == tomorrowPage) {                                                                 \
            time += 24 * 60;                                                                                 \
        }                                                                                                    \
        OSCalendarTime cal;                                                                                  \
        MinutesToCalendarTime(time, &cal);                                                                   \
        wchar_t* p = FormatNumber(cal.month + 1, belt->mText, 2, FALSE);                                       \
        *p = 0x6708; /* "month" */                                                                           \
        FormatNumber(cal.mday, p + 1, 2, FALSE);                                                             \
        wcscat(belt->mText, L"\x65E5"); /* "day" */                                                          \
        gTextWriter.SetFont(*gSysFont);                                                                      \
        gTextWriter.SetScale(1.0f);                                                                          \
        gTextWriter.SetCharSpace(0.0f);                                                                      \
        f32 width = gTextWriter.CalcStringWidth(belt->mText);                                                \
        f32 height = gTextWriter.CalcStringHeight(belt->mText);                                              \
        f32 scaleX = width > belt->mMaxWidth ? belt->mMaxWidth / width : 1.0f;                               \
        f32 scaleY = height > belt->mMaxHeight ? belt->mMaxHeight / height : 1.0f;                           \
        belt->mScaleX = scaleX;                                                                              \
        belt->mScaleY = scaleY;                                                                              \
    }

void WeatherNormal::SetupBeltJP() {
    SETUP_BELT(3, 2);
}

void WeatherNormal::SetupBelt() {
    SETUP_BELT(4, 3);
}

void WeatherNormal::StopScroll() {
    switch (mScrollPhase) {
    case 0:
        mScrollPhase++;
        mNameIndex = 0;
        break;
    case -1:
        break;
    default:
        SmoothApproach(&mNameScroll, 0.0f, 0.1f, 10.0f, 0.5f);
        break;
    }
}

// Scrolls through the lines of the city name
void WeatherNormal::ScrollNames() {
    switch (mScrollPhase) {
    case 0:
        mScrollPhase++;
        mNameIndex = 0;
        mScrollTimer = 240;
        mNameScroll = 0.0f;
        break;
    case -1:
        break;
    default: {
        if (mScrollTimer != 0) {
            mScrollTimer--;
        }
        f32 diff = SmoothApproach(&mNameScroll, sNameScrollY[mNameIndex], 0.1f, 10.0f, 0.5f);
        switch (mScrollPhase) {
        case 1:
            if (mScrollTimer == 0) {
                mScrollPhase++;
                mNameIndex++;
                if (mNameIndex >= mNumNames) {
                    mNameIndex = 0;
                }
                mScrollTimer = 240;
            }
            break;
        default:
        case 2:
            if (IsNearZero(diff)) {
                mScrollPhase = 1;
            }
            break;
        }
        break;
    }
    }
}

BOOL WeatherNormal::IsBackPressed() {
    f32 width = GetScreenWidth();
    for (s32 i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        if (IsPointerValid(i)) {
            f32 x = gCursorX[i];
            f32 y = gCursorY[i];
            if (x > 0.0f && x < width && y > 63.0f && y < 393.0f && (gTrig[i] & WPAD_BUTTON_A)) {
                return TRUE;
            }
        } else if (gTrig[i] & WPAD_BUTTON_A) {
            return TRUE;
        }
    }
    return FALSE;
}
