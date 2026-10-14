#include <channel/CityLabel.h>
#include <channel/DrawUtil.h>
#include <channel/System.h>
#include <channel/SimpleGlobe.h>
#include <channel/SceneBase.h>
#include <channel/ForecastData.h>
#include <channel/WeatherScene.h>
#include <wstring.h>

extern s32 gAmPm;
extern s32 gAmPmNext;

f32 SmoothApproach(f32* value, f32 target, f32 rate, f32 limit, f32 epsilon);
wchar_t* FormatInt(s32 value, wchar_t* out);
wchar_t* FormatNumber(s32 value, wchar_t* pBuf, s32 digits, BOOL zeroPad);

static Color sColorWhite(255, 255, 255, 255);
static Color sColorDarkGray(100, 100, 100, 255);
static Color sColorGray(140, 140, 140, 255);
static Color sColorRed(224, 31, 31, 255);
static Color sColorLightBlue(128, 128, 255, 255);
static Color sColorWhite2(255, 255, 255, 255);
static Color sColorHigh(223, 79, 94, 255);
static Color sColorLow(69, 165, 238, 255);
static Color sColorPale(223, 234, 249, 255);

typedef nw4r::ut::TextWriterBase<wchar_t> TextWriter;

static const wchar_t sDigits[] = L"0123456789";

static const wchar_t* sDayCaptions[] = {L"\x73FE\x5728 ", L"\x660E\x65E5 "}; // "now", "tomorrow"

CityLabel::CityLabel(City* city, f32 scale, nw4r::ut::Font* font)
    : mNext(NULL), mNextBack(NULL), mNextFront(NULL), mNextFrontBack(NULL), mNextOverlap(NULL), mCity(city),
      mBounds(0.0f, 0.0f, 0.0f, 0.0f), mShadowColor(sColorDarkGray), mState(NULL), mDrawFunc(NULL) {
    mPos.x = 0.0f;
    mPos.y = 0.0f;
    mBoxTarget.x = 0.0f;
    mBoxTarget.y = 0.0f;
    mLabelPos.x = 0.0f;
    mLabelPos.y = 0.0f;
    mSize.x = 0.0f;
    mSize.y = 0.0f;
    mOpenPos.x = 0.0f;
    mOpenPos.y = 0.0f;
    mBoxPos.x = 0.0f;
    mBoxPos.y = 0.0f;
    mFlags = 0;
    mInputFlags = 0;
    mBounce = 0;
    mOpen = 0.0f;
    mTextScale = 1.0f;
    mScale = 1.0f;
    mOpenAngle = 0.0f;
    mZoomFade = 1.0f;
    mEdgeFade = 1.0f;
    mColorR = 0.0f;
    mColorG = 0.0f;
    mColorB = 0.0f;
    mFade = 0.0f;
    mHighlight = 0.0f;
    mStateTimer = 0;
    unk27A = 0;
    mStateStep = 0;
    mAlpha = 0;
    unk27E = 0;
    mAlphaLow = 0;
    mPriority = ((PlaceEntry*)mCity->GetInfo()->mId)->unk14;

    mWriter.SetFont(*font);
    mWriter.SetDrawFlag(TextWriter::DRAWFLAG_ALIGN_TEXT_CENTER | TextWriter::DRAWFLAG_ALIGN_H_CENTER |
                        TextWriter::DRAWFLAG_ALIGN_V_CENTER);
    mWriter.SetScale(1.0f);
    mWriter.SetCharSpace(0.0f);
    mFontWidth = mWriter.GetFontWidth();
    mFontHeight = mWriter.GetFontHeight();
    mTomorrowWidth = mWriter.CalcStringWidth(sDayCaptions[1]);
    // Room for the "tomorrow" caption and the widest temperature line
    const wchar_t* tmpStr = L" 777\x2103(+77)";
    double tempWidth = mWriter.CalcStringWidth(tmpStr);
    f32 base = mFontWidth + (70.0f + mTomorrowWidth);
    mMinWidth = (f32)tempWidth + base;

    CityForecast* fc = mCity->GetForecast();
    BuildDayText(fc != NULL ? fc->mEntry->mDays : mCity->GetSummary()->mEntry->mDays);

    PlaceEntry* place = (PlaceEntry*)mCity->GetInfo()->mId;
    f32 lonSin, lonCos, latSin, latCos;
    nw4r::math::SinCosIdx(&lonSin, &lonCos, place->mLongitude);
    nw4r::math::SinCosIdx(&latSin, &latCos, ((PlaceEntry*)mCity->GetInfo()->mId)->mLatitude);
    mWorldPos.x = latSin * (scale * lonCos);
    mWorldPos.y = scale * lonSin;
    mWorldPos.z = latCos * (scale * lonCos);
    unk1D4[0].x = 0.0f;
    unk1D4[0].y = 0.0f;
    unk1D4[0].z = 0.0f;
    unk1D4[1].x = 0.0f;
    unk1D4[1].y = 0.0f;
    unk1D4[1].z = 0.0f;
    unk1D4[2].x = 0.0f;
    unk1D4[2].y = 0.0f;
    unk1D4[2].z = 0.0f;

    u32 id = mCity->GetInfo()->GetId();
    if (id == (u32)gCurrentCityId) {
        mPriority = 9;
    }

    // The outer side of each background box fades out
    mBgGradient0[0].Set(255, 255, 255, 0);
    mBgGradient0[1].Set(255, 255, 255, 0);
    mBgGradient0[2].Set(128, 255, 255, 255);
    mBgGradient0[3].Set(128, 255, 255, 255);
    mBgGradient1[0].Set(128, 255, 255, 255);
    mBgGradient1[1].Set(128, 255, 255, 255);
    mBgGradient1[2].Set(255, 255, 255, 0);
    mBgGradient1[3].Set(255, 255, 255, 0);
    ChangeState(&CityLabel::StateIdle);
}

CityLabel::~CityLabel() {}

static inline f32 Dot(const nw4r::math::VEC3* a, const nw4r::math::VEC3* b) {
    return nw4r::math::VEC3Dot(a, b);
}

// The camera position of the globe view, as a VEC3
static inline nw4r::math::VEC3* GetCameraPos(GlobeView* view) {
    return (nw4r::math::VEC3*)&view->mCameraPos;
}

void CityLabel::Project(GlobeView* view, s32 zoomLevel) {
    nw4r::math::VEC3* camera;
    nw4r::math::VEC3 normal = *(nw4r::math::VEC3*)&mWorldPos;
    nw4r::math::VEC3 dir;
    camera = GetCameraPos(view);
    dir = *(nw4r::math::VEC3*)&view->mTarget - *camera;
    f32 targetDist = nw4r::math::VEC3Len(&dir);
    view->Project(&mPos.x, &mWorldPos);

    u32 id = mCity->GetInfo()->GetId();
    if (id == gCurrentCityId) {
        mPriority = 9;
    } else {
        mPriority = ((PlaceEntry*)mCity->GetInfo()->mId)->unk14;
    }

    nw4r::math::VEC3Normalize(&normal, &normal);
    nw4r::math::VEC3Normalize(&dir, &dir);
    f32 facing = Dot(&normal, &dir);

    f32 zoomTarget;
    if (mPriority >= zoomLevel) {
        zoomTarget = 1.0f;
    } else {
        zoomTarget = 0.0f;
    }
    if (mZoomFade < zoomTarget) {
        mZoomFade += 0.05f;
        if (mZoomFade > zoomTarget) {
            mZoomFade = zoomTarget;
        }
    } else if (mZoomFade > zoomTarget) {
        mZoomFade -= 0.05f;
        if (mZoomFade < zoomTarget) {
            mZoomFade = zoomTarget;
        }
    }

    mPrevFlags = mFlags;
    mFlags = 0;
    f32 left = -50.0f;
    f32 right = 50.0f + GetScreenWidth();
    f32 top = -50.0f;
    f32 bottom = 506.0f;
    if (mPos.x >= left && mPos.x <= right && mPos.y >= top && mPos.y <= bottom && facing < 0.0f) {
        // How far the city is behind the point the camera looks at
        f32 dist = nw4r::math::FSqrt(nw4r::math::VEC3DistSq(camera, (nw4r::math::VEC3*)&mWorldPos));
        f32 depth = dist - targetDist;
        facing *= -3.0f;
        if (facing > 1.0f) {
            facing = 1.0f;
        }

        f32 fade;
        if (depth >= 3.0f) {
            fade = 0.0f;
        } else if (depth > 0.0f) {
            fade = 255.0f * nw4r::math::CosDeg(90.0f * (depth / 3.0f));
        } else {
            fade = 255.0f;
        }
        mAlpha = facing * (fade * mZoomFade);

        if (depth >= 1.0f) {
            mColorG -= 0.05f;
            if (mColorG < 0.0f) {
                mColorG = 0.0f;
            }
        } else {
            mColorG += 0.05f;
            if (mColorG > 1.0f) {
                mColorG = 1.0f;
            }
        }

        if (mAlpha != 0) {
            mFlags |= CITY_LABEL_ON_SCREEN;
        }
    } else {
        mAlpha = 0;
        mColorG = 0.0f;
    }

    UpdateBorderFade();
}

static inline BOOL IsWeatherMissing(u16 weather) {
    if (weather != 0xFFFF) {
        u32 code = weather;
        if (gForecastData->FindWeatherInfo(code) != NULL) {
            return FALSE;
        }
    }
    return TRUE;
}

static inline void FormatTempText(wchar_t* out, s8 temp) {
    if (temp > -128) {
        FormatInt(temp, out);
        if (gLanguage != 0 && gRegion != 1) {
            wcscat(out, L"\xFF9F"); // degree mark
        }
    } else {
        wcscpy(out, L"--");
    }
}

static inline void FormatDiffText(wchar_t* out, const s8* diff, BOOL sign) {
    wchar_t* p = out;
    if (*diff > -128) {
        if (sign && *diff >= 0) {
            *p++ = L'+';
        }
        FormatInt(*diff, p);
    } else {
        wcscpy(out, L"--");
    }
}

static inline void FormatPercentText(wchar_t* out, u8 percent) {
    if (percent != 0xFF) {
        FormatInt(percent, out);
    } else {
        wcscpy(out, L"--");
    }
}

static inline void FormatNowText(wchar_t* out, s32 temp) {
    if (temp > -128) {
        FormatNumber(temp, out, 4, FALSE);
        if (gRegion != 1) {
            wcscat(out, L"\xFF9F"); // degree mark
        }
    } else {
        wcscpy(out, L"--");
    }
}

void CityLabel::BuildDayText(DayForecast* days) {
    s32 first, second;
    DayForecast* day = days;
    for (s32 i = 0; i < 2; i++, day++) {
        if (IsWeatherMissing(day->mWeather)) {
            mDays[i].mMaxC = -128;
            wcscpy(mDays[i].mMax[0], L"--");
            wcscpy(mDays[i].mMaxDiff[0], L"--");
            wcscpy(mDays[i].mMin[0], L"--");
            wcscpy(mDays[i].mMinDiff[0], L"--");
            wcscpy(mDays[i].mMax[1], L"--");
            wcscpy(mDays[i].mMaxDiff[1], L"--");
            wcscpy(mDays[i].mMin[1], L"--");
            wcscpy(mDays[i].mMinDiff[1], L"--");
            wcscpy(mDays[i].mPercent[0], L"--");
            wcscpy(mDays[i].mPercent[1], L"--");
            continue;
        }

        s8 maxC = day->mMaxC;
        mDays[i].mMaxC = maxC;
        FormatTempText(mDays[i].mMax[0], maxC);
        FormatDiffText(mDays[i].mMaxDiff[0], &day->mMaxDiffC, TRUE);
        FormatTempText(mDays[i].mMin[0], day->mMinC);
        FormatDiffText(mDays[i].mMinDiff[0], &day->mMinDiffC, FALSE);
        FormatTempText(mDays[i].mMax[1], day->mMaxF);
        FormatDiffText(mDays[i].mMaxDiff[1], &day->mMaxDiffF, TRUE);
        FormatTempText(mDays[i].mMin[1], day->mMinF);
        FormatDiffText(mDays[i].mMinDiff[1], &day->mMinDiffF, FALSE);

        // Today shows the chances for the current and next part of the day
        if (i == 0) {
            first = gAmPm;
            second = gAmPmNext;
        } else {
            first = 1;
            second = 2;
        }
        FormatPercentText(mDays[i].mPercent[0], day->mPercent[first]);
        FormatPercentText(mDays[i].mPercent[1], day->mPercent[second]);
    }

    CityNow* now = mCity->mNow;
    if (now != NULL && !IsWeatherMissing(now->mEntry->mWeather)) {
        s8 tempC = now->mEntry->mTempC;
        mNowC = tempC;
        wchar_t* text = mNowText[0];
        FormatNowText(text, tempC);
        text = mNowText[1];
        FormatNowText(text, now->mEntry->mTempF);
    } else {
        mNowC = -128;
        wcscpy(mNowText[0], L"--");
        wcscpy(mNowText[1], L"--");
    }
}

// Writes "max<unit>/min<unit>" through the caller's p. The second unit lands on the start of the
// minimum, not after it: the original drops FormatTemp's return value there.
#define FORMAT_MAX_MIN(out, max, min, unit) \
    do {                                    \
        p = FormatTemp(out, max);           \
        *p++ = unit;                        \
        *p++ = L'/';                        \
        FormatTemp(p, min);                 \
        p[0] = unit;                        \
        p[1] = 0;                           \
    } while (0)

void CityLabel::UpdateTempText() {
    CityForecast* forecast = mCity->GetForecast();
    CitySummary* summary = mCity->GetSummary();
    if (forecast == NULL && summary == NULL) {
        return;
    }

    wchar_t* p;
    s32 max;
    s32 min;
    s32 nextMin;

    if (gTempUnit == 0) {
        if (forecast != NULL) {
            ForecastEntry* entry = forecast->mEntry;
            max = entry->mDays[0].mMaxC;
            min = entry->mDays[0].mMinC;
            nextMin = entry->mDays[1].mMinC;
            FORMAT_MAX_MIN(mTempText[1], entry->mDays[1].mMaxC, nextMin, L'\x2103'); // degrees Celsius
        } else {
            SummaryEntry* entry = summary->mEntry;
            max = entry->mDays[0].mMaxC;
            min = entry->mDays[0].mMinC;
            mTempText[1][0] = L'-';
            mTempText[1][1] = 0;
        }
        FORMAT_MAX_MIN(mTempText[0], max, min, L'\x2103');
    } else {
        if (forecast != NULL) {
            ForecastEntry* entry = forecast->mEntry;
            max = entry->mDays[0].mMaxF;
            min = entry->mDays[0].mMinF;
            nextMin = entry->mDays[1].mMinF;
            FORMAT_MAX_MIN(mTempText[1], entry->mDays[1].mMaxF, nextMin, L'\xFF26'); // full-width F
        } else {
            SummaryEntry* entry = summary->mEntry;
            max = entry->mDays[0].mMaxF;
            min = entry->mDays[0].mMinF;
            mTempText[1][0] = L'-';
            mTempText[1][1] = 0;
        }
        FORMAT_MAX_MIN(mTempText[0], max, min, L'\xFF26');
    }
}

// Writes a temperature of up to three digits, returns the end of the string
wchar_t* CityLabel::FormatTemp(wchar_t* out, s32 temp) {
    s32 rest;
    s32 hundreds = temp / 100;
    rest = temp % 100;
    s32 tens = rest / 10;
    s32 ones = rest % 10;
    if (temp < 0) {
        *out++ = L'-';
    }
    if (hundreds != 0) {
        out[0] = sDigits[hundreds];
        out[1] = sDigits[tens];
        out[2] = sDigits[ones];
        out += 3;
    } else if (tens != 0) {
        out[0] = sDigits[tens];
        out[1] = sDigits[ones];
        out += 2;
    } else {
        out[0] = sDigits[ones];
        out += 1;
    }
    *out = 0;
    return out;
}

// Fades the label out while it is near any edge of the screen
void CityLabel::UpdateBorderFade() {
    f32 margin = 63.0f;
    f32 limit = 456.0f - margin;
    f32 left = (f32)(gWidescreen ? 36 : 28);
    f32 right = (f32)(GetScreenWidth() - (gWidescreen ? 36 : 28));
    f32 half = 0.5f * (gLabelIconSize * mScale);
    f32 top = mPos.y - half;
    f32 bottom = mPos.y + half;
    if (mPos.x < left || top < margin || mPos.x > right || bottom > limit) {
        mColorB -= 0.2f;
        if (mColorB < 0.0f) {
            mColorB = 0.0f;
        }
    } else {
        mColorB += 0.2f;
        if (mColorB > 1.0f) {
            mColorB = 1.0f;
        }
    }
}

void CityLabel::CheckPointer() {
    if (mInputFlags & CITY_LABEL_HIT) {
        f32 x;
        f32 y;
        nw4r::math::VEC2 half;
        half = *(nw4r::math::VEC2*)&mSize * 0.5f;
        f32 left = mBoxPos.x - half.x;
        f32 right = mBoxPos.x + half.x;
        f32 top = mBoxPos.y - half.y;
        f32 bottom = mBoxPos.y + half.y;
        for (s32 i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
            if (gKPADLatest[i] >= 0) {
                x = gCursorX[i];
                y = gCursorY[i];
                if (x >= left && x <= right && y >= top && y <= bottom) {
                    mFlags |= CITY_LABEL_POINTED;
                    return;
                }
            }
        }
    }
}

void CityLabel::UpdateHover() {
    BOOL hovered = FALSE;
    if ((mFlags & CITY_LABEL_ON_SCREEN) && (mInputFlags & CITY_LABEL_SELECTED)) {
        hovered = TRUE;
    }
    if (hovered) {
        mColorR -= 0.1f;
        if (mColorR < 0.0f) {
            mColorR = 0.0f;
        }
    } else {
        mColorR += 0.1f;
        if (mColorR > 1.0f) {
            mColorR = 1.0f;
        }
    }

    if ((mFlags & CITY_LABEL_ON_SCREEN) && ((mFlags & CITY_LABEL_HOVERED) || (mInputFlags & CITY_LABEL_SELECTED))) {
        if (mBounce < 0x8000) {
            mBounce += 0x1800;
            if (mBounce >= 0x8000) {
                mBounce = 0x8000;
                PlaySE(1);
            }
        }
    } else if (mBounce > 0) {
        mBounce -= 0x1800;
        if (mBounce < 0) {
            mBounce = 0;
        }
    }

    if (mState != NULL) {
        (this->*mState)();
    }
}

void CityLabel::UpdateAlpha() {
    if (mFlags & CITY_LABEL_HIDDEN) {
        mFade -= 0.1f;
        if (mFade < 0.0f) {
            mFade = 0.0f;
        }
    } else {
        mFade += 0.1f;
        if (mFade > 1.0f) {
            mFade = 1.0f;
        }
    }

    mAlphaLow = mAlpha * mColorR * mColorG * mColorB;
}

void CityLabel::ResetDrawFunc() {
    mDrawFunc = &CityLabel::UpdateBox;
}

// Places the open box beside the label, on the side with more room, and slides it out
void CityLabel::UpdateBox() {
    // The declaration order sets the register allocation order; keep it
    f32 boxLeft, boxX, boxRight, leftBoxX, leftBoxRight, edgeR, y, edgeL, rightX;
    f32 halfW, roomL, left, right, top, leftX, roomR, bottom;
    top = 40.0f + (f32)(gWidescreen ? 19 : 34);
    bottom = (f32)(456 - (gWidescreen ? 19 : 34)) - 40.0f;
    left = (f32)(gWidescreen ? 36 : 28);
    right = (f32)(GetScreenWidth() - (gWidescreen ? 36 : 28));
    halfW = 0.5f * mSize.x;
    y = 70.0f + mPos.y;
    edgeR = mPos.x + halfW;
    roomR = right - edgeR;
    leftX = mPos.x - 70.0f;
    rightX = 70.0f + mPos.x;
    edgeL = mPos.x - halfW;
    roomL = edgeL - left;
    if (roomL > roomR) {
        leftBoxX = leftX - halfW;
        mBoxTarget.x = leftBoxX;
        leftBoxRight = leftBoxX + halfW;
        if (leftBoxRight > right) {
            mBoxTarget.x = right - halfW;
        }
        if (mBoxTarget.x - halfW < left) {
            mBoxTarget.x = left + halfW;
        }
    } else {
        boxX = rightX + halfW;
        mBoxTarget.x = boxX;
        boxLeft = boxX - halfW;
        boxRight = boxX + halfW;
        if (boxRight > right) {
            boxX = right - halfW;
            mBoxTarget.x = boxX;
            boxLeft = boxX - halfW;
        }
        if (boxLeft < left) {
            mBoxTarget.x = left + halfW;
        }
    }

    f32 halfH = 0.5f * mSize.y;
    if (y < top) {
        mBoxTarget.y = top + halfH;
    } else {
        if (y + mSize.y > bottom) {
            mBoxTarget.y = bottom - halfH;
        } else {
            mBoxTarget.y = y + halfH;
        }
        if (mBoxTarget.y - halfH < top) {
            mBoxTarget.y = top + halfH;
        }
    }

    const nw4r::math::VEC2* boxTarget = (const nw4r::math::VEC2*)&mBoxTarget;
    mOpenPos.x = boxTarget->x;
    mOpenPos.y = boxTarget->y;
    nw4r::math::VEC2 diff;
    diff = *boxTarget - *(nw4r::math::VEC2*)&mPos;
    mBoxPos = Vector2(*(nw4r::math::VEC2*)&mPos + diff * mOpen);
    unk1D4[0].x = mBoxPos.x;
    unk1D4[0].y = mBoxPos.y;
    unk1D4[1].x = mBoxPos.x;
    unk1D4[1].y = mBoxPos.y;
    unk1D4[2].x = mPos.x;
    unk1D4[2].y = mPos.y;

    CheckPointer();
    if (mFlags & CITY_LABEL_POINTED) {
        SmoothApproach(&mHighlight, 1.0f, 0.25f, 0.5f, 0.001f);
    } else {
        SmoothApproach(&mHighlight, 0.0f, 0.25f, 0.5f, 0.001f);
    }
    s32 red = sColorGray.r + (s32)(mHighlight * (sColorDarkGray.r - sColorGray.r));
    s32 green = sColorGray.g + (s32)(mHighlight * (sColorDarkGray.g - sColorGray.g));
    s32 blue = sColorGray.b + (s32)(mHighlight * (sColorDarkGray.b - sColorGray.b));
    u8 darkA = sColorDarkGray.a;
    mShadowColor.r = red;
    mShadowColor.g = green;
    mShadowColor.b = blue;
    mShadowColor.a = darkA;
}

// The label is as wide as the longer of the city name and "country region"
void CityLabel::CalcSize() {
    // Unused, but the original has a 20.0f here: it puts the constant at this spot of the pool
    f32 padding = 20.0f;
    Rect nameRect;
    Rect rect;
    mWriter.SetScale(1.0f);
    mWriter.SetCharSpace(0.0f);
    mWriter.CalcStringRect((nw4r::ut::Rect*)&nameRect, mCity->GetInfo()->mName);

    f32 width;
    if (mCity->GetInfo()->mCountry != NULL) {
        mWriter.CalcStringRect((nw4r::ut::Rect*)&rect, mCity->GetInfo()->mCountry);
        width = rect.GetWidth();
    } else {
        width = 0.0f;
    }
    if (mCity->GetInfo()->mRegion != NULL) {
        mWriter.CalcStringRect((nw4r::ut::Rect*)&rect, mCity->GetInfo()->mRegion);
        f32 regionWidth = rect.GetWidth();
        if (regionWidth > 0.0f) {
            width += regionWidth;
            mWriter.CalcStringRect((nw4r::ut::Rect*)&rect, L"\x3000"); // full-width space
            width += rect.GetWidth();
        }
    }
    if (width < nameRect.GetWidth()) {
        width = nameRect.GetWidth();
    }

    mSize.x = width + mWriter.GetFontWidth();
    if (mSize.x < mMinWidth) {
        mSize.x = mMinWidth;
    }
    mSize.y = 10.5f * mFontHeight;
}

void CityLabel::DrawBg() {
    SetDefaultGXState();
    SetOrthoProjection();
    DrawGradientRect(&mBgRects[0], (const GXColor*)mBgGradient0);
    DrawGradientRect(&mBgRects[1], (const GXColor*)mBgGradient1);
}

void CityLabel::DrawName() {
    if (mFlags & CITY_LABEL_ON_SCREEN) {
        u8 alpha = mFade * mAlphaLow;
        SetDefaultGXState();
        SetOrthoProjection();
        mWriter.SetDrawFlag(TextWriter::DRAWFLAG_ALIGN_V_CENTER);
        mWriter.SetupGX();
        mWriter.SetScale(mTextScale);
        mWriter.SetCharSpace(0.0f);
        mWriter.SetCursor(mLabelPos.x, mLabelPos.y);
        mWriter.SetTextColor(nw4r::ut::Color(255, 255, 255, alpha));
        mWriter.Print(mCity->GetInfo()->mName);
    }
}

void CityLabel::DrawDayTemp(s32 day, f32 scale) {
    nw4r::ut::Color color = GetTempColor(mDays[day].mMaxC);
    nw4r::ut::Color shadow(255, 255, 255, mAlpha);
    f32 alphaLow = mAlphaLow; // unused; its int-to-float stores stay in the code
    Vector2 pos(mPos.x, mPos.y);
    color.a = mAlpha;
    SetDefaultGXState();
    SetOrthoProjection();
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C1, GX_CC_TEXC, GX_CC_C0, GX_CC_C1);
    shadow.r = gColorDarkGray3.r;
    shadow.g = gColorDarkGray3.g;
    shadow.b = gColorDarkGray3.b;
    DrawTempCentered(mDays[day].mMax[gTempUnit], &pos, scale, scale, &color, &shadow);
}

// The max and min temperatures in a box, or just the temperature when zoomed in
void CityLabel::DrawTempAlt(s32 day, f32 scale) {
    if (gSimpleGlobe != NULL && gSimpleGlobe->mZoomLevel > gLabelDetailZoom) {
        DrawDayTemp(day, scale);
        return;
    }

    f32 minX, minY, maxX, maxY;
    f32 size = 96.0f * scale;
    f32 half = 0.5f * size;
    f32 quarter = 0.5f * half;
    f32 textScale = 0.75f * scale;
    Vec quad[4];
    Vec2 corners[2]; // top left and bottom right, passed to DrawRectOutline as a Rect
    Vec2 pos;
    nw4r::ut::Color top(sColorHigh.r, sColorHigh.g, sColorHigh.b, mAlpha);
    nw4r::ut::Color bottom(sColorLow.r, sColorLow.g, sColorLow.b, mAlpha);
    nw4r::ut::Color outline(32, 32, 32, mAlpha);
    f32 alphaLow = mAlphaLow; // unused; its int-to-float stores stay in the code
    f32 py = mPos.y - quarter;
    pos.x = mPos.x;
    pos.y = py;
    minY = mPos.y - half;
    maxX = mPos.x + half;
    minX = mPos.x - half;
    maxY = mPos.y + half;
    corners[0].y = minY;
    corners[0].x = minX;
    corners[1].x = maxX;
    corners[1].y = maxY;
    quad[0].z = quad[1].z = quad[2].z = quad[3].z = 0.0f;
    quad[0].x = quad[1].x = corners[0].x;
    quad[2].x = quad[3].x = corners[1].x;
    quad[0].y = quad[3].y = corners[0].y;
    quad[1].y = quad[2].y = mPos.y;

    SetDefaultGXState();
    SetOrthoProjection();
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
    DrawQuad(quad, &top);
    quad[0].y = quad[3].y = mPos.y;
    quad[1].y = quad[2].y = corners[1].y;
    DrawQuad(quad, &bottom);
    // Passed as the plain GXColor: copied as one word
    DrawRectOutline((const Rect*)corners, static_cast<GXColor&>(outline), 12);

    SetDefaultGXState();
    SetOrthoProjection();
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C1, GX_CC_TEXC, GX_CC_C0, GX_CC_C1);
    const wchar_t* line1 = mDays[day].mMax[gTempUnit];
    const wchar_t* line2 = mDays[day].mMin[gTempUnit];
    f32 len1 = wcslen(line1);
    f32 len2 = wcslen(line2);

    // A single character is moved right by half its width
    f32 offset = 1.0f == len1 ? 0.5f * (textScale * CalcNumWidth(line1, 0.0f)) : 0.0f;
    pos.x = mPos.x + offset;
    top.r = gColorPink.r;
    top.g = gColorPink.g;
    top.b = gColorPink.b;
    bottom.r = gColorRed2.r;
    bottom.g = gColorRed2.g;
    bottom.b = gColorRed2.b;
    bottom.a = 0;
    DrawNumCentered(line1, &pos, textScale, textScale, 0.0f, &top, &bottom);

    offset = 1.0f == len2 ? 0.5f * (textScale * CalcNumWidth(line2, 0.0f)) : 0.0f;
    pos.x = mPos.x + offset;
    pos.y = mPos.y + quarter;
    top.r = gColorCyan2.r;
    top.g = gColorCyan2.g;
    top.b = gColorCyan2.b;
    bottom.r = gColorBlue.r;
    bottom.g = gColorBlue.g;
    bottom.b = gColorBlue.b;
    bottom.a = 0;
    DrawNumCentered(line2, &pos, textScale, textScale, 0.0f, &top, &bottom);
}

// The max temperature and its change in a box, or just the temperature when zoomed in
void CityLabel::DrawTemp(s32 day, f32 scale) {
    if (gSimpleGlobe != NULL && gSimpleGlobe->mZoomLevel > gLabelDetailZoom) {
        DrawDayTemp(day, scale);
        return;
    }

    f32 minX, minY, maxX, maxY;
    f32 size = 96.0f * scale;
    f32 half = 0.5f * size;
    f32 quarter = 0.5f * half;
    f32 textScale = 0.75f * scale;
    Vec quad[4];
    Vec2 corners[2]; // top left and bottom right, passed to DrawRectOutline as a Rect
    Vec2 pos;
    nw4r::ut::Color top(sColorHigh.r, sColorHigh.g, sColorHigh.b, mAlpha);
    nw4r::ut::Color bottom(sColorPale.r, sColorPale.g, sColorPale.b, mAlpha);
    nw4r::ut::Color outline(32, 32, 32, mAlpha);
    f32 alphaLow = mAlphaLow; // unused; its int-to-float stores stay in the code
    f32 py = mPos.y - quarter;
    pos.x = mPos.x;
    pos.y = py;
    minY = mPos.y - half;
    maxX = mPos.x + half;
    minX = mPos.x - half;
    maxY = mPos.y + half;
    corners[0].y = minY;
    corners[0].x = minX;
    corners[1].x = maxX;
    corners[1].y = maxY;
    quad[0].z = quad[1].z = quad[2].z = quad[3].z = 0.0f;
    quad[0].x = quad[1].x = corners[0].x;
    quad[2].x = quad[3].x = corners[1].x;
    quad[0].y = quad[3].y = corners[0].y;
    quad[1].y = quad[2].y = mPos.y;

    SetDefaultGXState();
    SetOrthoProjection();
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
    DrawQuad(quad, &top);
    quad[0].y = quad[3].y = mPos.y;
    quad[1].y = quad[2].y = corners[1].y;
    DrawQuad(quad, &bottom);
    // Passed as the plain GXColor: copied as one word
    DrawRectOutline((const Rect*)corners, static_cast<GXColor&>(outline), 12);

    SetDefaultGXState();
    SetOrthoProjection();
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C1, GX_CC_TEXC, GX_CC_C0, GX_CC_C1);
    const wchar_t* line1 = mDays[day].mMax[gTempUnit];
    const wchar_t* line2 = mDays[day].mMaxDiff[gTempUnit];
    f32 len1 = wcslen(line1);
    f32 len2 = wcslen(line2);

    // A single character is moved right by half its width
    f32 offset = 1.0f == len1 ? 0.5f * (textScale * CalcNumWidth(line1, 0.0f)) : 0.0f;
    pos.x = mPos.x + offset;
    top.r = gColorPink.r;
    top.g = gColorPink.g;
    top.b = gColorPink.b;
    bottom.r = gColorRed2.r;
    bottom.g = gColorRed2.g;
    bottom.b = gColorRed2.b;
    bottom.a = 0;
    DrawNumCentered(line1, &pos, textScale, textScale, 0.0f, &top, &bottom);

    offset = 1.0f == len2 ? 0.5f * (textScale * CalcNumWidth(line2, 0.0f)) : 0.0f;
    pos.x = mPos.x + offset;
    pos.y = mPos.y + quarter;
    top.r = gColorWhite2.r;
    top.g = gColorWhite2.g;
    top.b = gColorWhite2.b;
    bottom.r = gColorDarkGray3.r;
    bottom.g = gColorDarkGray3.g;
    bottom.b = gColorDarkGray3.b;
    bottom.a = 0;
    DrawNumCentered(line2, &pos, textScale, textScale, 0.0f, &top, &bottom);
}

void CityLabel::DrawRain(s32 day, f32 scale) {
    f32 textScale = scale; // a plain copy, but it sets the original instruction order
    nw4r::ut::Color color;
    nw4r::ut::Color shadow(255, 255, 255, mAlpha);
    f32 alphaLow = mAlphaLow; // unused; its int-to-float stores stay in the code
    Vector2 pos(mPos.x, mPos.y);
    color = GetTempColor(mDays[day].mMaxC);
    color.a = mAlpha;
    SetDefaultGXState();
    SetOrthoProjection();
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C1, GX_CC_TEXC, GX_CC_C0, GX_CC_C1);
    shadow.r = gColorDarkGray3.r;
    shadow.g = gColorDarkGray3.g;
    shadow.b = gColorDarkGray3.b;
    DrawTempCentered(mDays[day].mMax[gTempUnit], &pos, textScale, textScale, &color, &shadow);
}

void CityLabel::DrawHigh(f32 scale) {
    nw4r::ut::Color color(255, 104, 104, mAlpha);
    nw4r::ut::Color shadow(255, 255, 255, mAlpha);
    f32 alphaLow = mAlphaLow; // unused; its int-to-float stores stay in the code
    Vector2 pos(mPos.x, mPos.y);
    color = GetTempColor(mNowC);
    color.a = mAlpha;
    SetDefaultGXState();
    SetOrthoProjection();
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C1, GX_CC_TEXC, GX_CC_C0, GX_CC_C1);
    shadow.r = gColorDarkGray3.r;
    shadow.g = gColorDarkGray3.g;
    shadow.b = gColorDarkGray3.b;
    DrawTempCentered(mNowText[gTempUnit], &pos, scale, scale, &color, &shadow);
}

// The box with the day's two precipitation chances, drawn over the open label
void CityLabel::DrawDetail(BOOL tomorrow, f32 scale) {
    f32 minX, minY, maxX, maxY;
    f32 size = 96.0f * scale;
    f32 half = 0.5f * size;
    f32 quarter = 0.5f * half;
    f32 textScale = 0.75f * scale;
    Vec quad[4];
    Vec2 corners[2]; // top left and bottom right, passed to DrawRectOutline as a Rect
    Vec2 pos;
    nw4r::ut::Color top(80, 80, 192, mAlpha);
    nw4r::ut::Color bottom(100, 100, 255, mAlpha);
    GXColor outline;
    outline.r = 32;
    outline.g = 32;
    outline.b = 32;
    outline.a = mAlpha;
    f32 alphaLow = mAlphaLow; // unused; its int-to-float stores stay in the code
    f32 py = mPos.y - quarter;
    pos.x = mPos.x;
    pos.y = py;
    minY = mPos.y - half;
    maxX = mPos.x + half;
    minX = mPos.x - half;
    maxY = mPos.y + half;
    corners[0].y = minY;
    corners[0].x = minX;
    corners[1].x = maxX;
    corners[1].y = maxY;
    quad[0].z = quad[1].z = quad[2].z = quad[3].z = 0.0f;
    quad[0].x = quad[1].x = corners[0].x;
    quad[2].x = quad[3].x = corners[1].x;
    quad[0].y = quad[3].y = corners[0].y;
    quad[1].y = quad[2].y = mPos.y;

    SetDefaultGXState();
    SetOrthoProjection();
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
    DrawQuad(quad, &top);
    quad[0].y = quad[3].y = mPos.y;
    quad[1].y = quad[2].y = corners[1].y;
    DrawQuad(quad, &bottom);
    DrawRectOutline((const Rect*)corners, outline, 12);

    SetDefaultGXState();
    SetOrthoProjection();
    const wchar_t* line1 = mDays[tomorrow].mPercent[0];
    const wchar_t* line2 = mDays[tomorrow].mPercent[1];
    f32 len1 = wcslen(line1);
    f32 len2 = wcslen(line2);

    // A single character is moved right by half its width
    f32 offset = 1.0f == len1 ? 0.5f * (textScale * CalcDateWidth(line1)) : 0.0f;
    pos.x = mPos.x + offset;
    top.r = gColorWhite2.r;
    top.g = gColorWhite2.g;
    top.b = gColorWhite2.b;
    bottom.r = gColorDarkGray3.r;
    bottom.g = gColorDarkGray3.g;
    bottom.b = gColorDarkGray3.b;
    DrawDateCentered(line1, &pos, textScale, textScale, &top, &bottom);

    offset = 1.0f == len2 ? 0.5f * (textScale * CalcDateWidth(line2)) : 0.0f;
    pos.x = mPos.x + offset;
    pos.y = mPos.y + quarter;
    DrawDateCentered(line2, &pos, textScale, textScale, &top, &bottom);
}

// From blue for the cold to red for the hot, blended between fixed points
nw4r::ut::Color CityLabel::GetTempColor(s8 temp) {
    static Color sColors[] = {
        Color(255, 255, 255, 255), Color(187, 255, 255, 255), Color(0, 255, 255, 255), Color(33, 165, 227, 255),
        Color(0, 102, 205, 255),   Color(140, 255, 0, 255),   Color(255, 255, 0, 255), Color(255, 182, 0, 255),
        Color(255, 140, 0, 255),   Color(255, 98, 32, 255),   Color(189, 0, 0, 255),
    };

    if (temp <= -18) {
        return sColors[0];
    } else if (temp <= -12) {
        return BlendColor(sColors[0], sColors[1], 6.0f, -12 - temp);
    } else if (temp <= -7) {
        return BlendColor(sColors[1], sColors[2], 5.0f, -7 - temp);
    } else if (temp <= -1) {
        return BlendColor(sColors[2], sColors[3], 6.0f, -1 - temp);
    } else if (temp <= 4) {
        return BlendColor(sColors[3], sColors[4], 5.0f, 4 - temp);
    } else if (temp <= 10) {
        return BlendColor(sColors[4], sColors[5], 6.0f, 10 - temp);
    } else if (temp <= 16) {
        return BlendColor(sColors[5], sColors[6], 6.0f, 16 - temp);
    } else if (temp <= 21) {
        return BlendColor(sColors[6], sColors[7], 5.0f, 21 - temp);
    } else if (temp <= 27) {
        return BlendColor(sColors[7], sColors[8], 6.0f, 27 - temp);
    } else if (temp <= 32) {
        return BlendColor(sColors[8], sColors[9], 5.0f, 32 - temp);
    }
    return sColors[10];
}

// Fades from one color towards another, reaching it when time equals duration
nw4r::ut::Color CityLabel::BlendColor(const Color& to, const Color& from, f32 duration, f32 time) {
    nw4r::ut::Color color(0xFFFFFFFF);
    f32 t = time / duration;
    f32 r = from.r - to.r;
    f32 g = from.g - to.g;
    f32 b = from.b - to.b;
    color.r = from.r - (s32)(r * t);
    color.g = from.g - (s32)(g * t);
    color.b = from.b - (s32)(b * t);
    return color;
}

void CityLabel::SetPosition(Vec2* pos, f32 scale) {
    mLabelPos.x = pos->x;
    mLabelPos.y = pos->y;
    mTextScale = scale;
    UpdateBounds();

    SmoothApproach(&mBgAlpha, (mFlags & CITY_LABEL_HOVERED) ? 1.0f : 0.0f, 0.2f, 1.0f, 0.05f);
    u8 alpha = 255.0f * mBgAlpha;
    mBgGradient0[0].a = mBgGradient0[1].a = mBgGradient1[2].a = mBgGradient1[3].a = 0;
    mBgGradient0[2].a = mBgGradient0[3].a = mBgGradient1[0].a = mBgGradient1[1].a = alpha;
}

// Measures the city name and places the bounds and the two background boxes around it
void CityLabel::UpdateBounds() {
    Rect rect; // unused, but the original zeroes it on the stack
    mWriter.SetScale(mTextScale);
    mWriter.SetCharSpace(0.0f);
    mWriter.CalcStringRect((nw4r::ut::Rect*)&mBounds, mCity->GetInfo()->mName);

    f32 y = mLabelPos.y - 0.5f * mBounds.GetHeight();
    mBounds.left += mLabelPos.x - 4.0f;
    mBounds.top += y - 4.0f;
    mBounds.right += 4.0f + mLabelPos.x;
    mBounds.bottom += 4.0f + y;

    mBgRects[0].top = mBgRects[1].top = mBounds.top;
    mBgRects[0].bottom = mBgRects[1].bottom = mBounds.bottom;
    mBgRects[0].left = mBounds.left - 100.0f;
    mBgRects[0].right = mBgRects[1].left = mBounds.left;
    mBgRects[1].right = 50.0f + mBounds.right;

    if (mFlags & CITY_LABEL_HOVERED) {
        mBounds.left = mBgRects[0].left;
        mBounds.right = mBgRects[1].right;
    }
}

BOOL CityLabel::IsDrawn() {
    BOOL result = FALSE;
    BOOL visible = FALSE;
    BOOL onScreen = FALSE;
    u32 flags = mFlags;
    if ((flags & CITY_LABEL_ON_SCREEN) && !(flags & CITY_LABEL_HIDDEN)) {
        onScreen = TRUE;
    }
    if (onScreen) {
        if (mAlphaLow != 0) {
            visible = TRUE;
        }
    }

    if (visible) {
        BOOL nearZero = FALSE;
        if (mTextScale < 0.0008f) {
            if (mTextScale > -0.0008f) {
                nearZero = TRUE;
            }
        }
        if (!nearZero) {
            result = TRUE;
        }
    }
    return result;
}

BOOL CityLabel::StateIdle() {
    BOOL selected = FALSE;
    if ((mFlags & CITY_LABEL_ON_SCREEN) && (mInputFlags & CITY_LABEL_SELECTED)) {
        selected = TRUE;
    }

    switch (mStateStep) {
    case 0:
        mStateStep++;
        mInputFlags &= ~CITY_LABEL_HIT;
        mStateTimer = 0;
        // fall through: the first frame of a state also runs its update
    default:
        if (selected) {
            if (mStateTimer == 0) {
                ChangeState(&CityLabel::StateOpen);
            } else {
                mStateTimer--;
            }
        } else {
            mStateStep = 0;
        }
        break;
    case -1:
        break;
    }
    return TRUE;
}

BOOL CityLabel::StateOpen() {
    switch (mStateStep) {
    case 0:
        mStateStep++;
        mInputFlags |= CITY_LABEL_HIT;
        mBoxPos.x = mPos.x;
        mBoxPos.y = mPos.y;
        mOpenPos.x = mPos.x;
        mOpenPos.y = mPos.y;
        CallDrawFunc();
        // fall through: the first frame of a state also runs its update
    default:
        unk27E = 255;
        mOpenAngle += 9.0f;
        if (mOpenAngle > 90.0f) {
            mOpenAngle = 90.0f;
        }
        mOpen = nw4r::math::SinDeg(mOpenAngle);
        if (90.0f == mOpenAngle) {
            ChangeState(&CityLabel::StateOpened);
            return TRUE;
        }
        CallDrawFunc();
        break;
    case -1:
        break;
    }
    return TRUE;
}

BOOL CityLabel::StateOpened() {
    BOOL selected = FALSE;
    if ((mFlags & CITY_LABEL_ON_SCREEN) && (mInputFlags & CITY_LABEL_SELECTED)) {
        selected = TRUE;
    }

    switch (mStateStep) {
    case 0:
        mStateStep++;
        mStateTimer = 0;
        // fall through: the first frame of a state also runs its update
    default:
        if (!selected) {
            if (mStateTimer == 0) {
                ChangeState(&CityLabel::StateClose);
                return TRUE;
            }
            mStateTimer--;
        } else {
            mStateStep = 0;
        }
        CallDrawFunc();
        break;
    case -1:
        break;
    }
    return TRUE;
}

BOOL CityLabel::StateClose() {
    switch (mStateStep) {
    case 0:
        mStateStep++;
        // fall through: the first frame of a state also runs its update
    default:
        mOpenAngle -= 9.0f;
        if (mOpenAngle < 0.0f) {
            mOpenAngle = 0.0f;
        }
        mOpen = nw4r::math::SinDeg(mOpenAngle);
        if (0.0f == mOpenAngle) {
            unk27E = 0;
            ChangeState(&CityLabel::StateIdle);
            return TRUE;
        }
        CallDrawFunc();
        break;
    case -1:
        break;
    }
    return TRUE;
}

void CityLabel::UpdateEdgeFade() {
    // Dim the label while its icon is within 63 of the top or bottom of the screen
    f32 margin = 63.0f;
    f32 limit = 456.0f - margin;
    f32 half = 0.5f * (gLabelIconSize * mScale);
    f32 top = mPos.y - half;
    f32 bottom = mPos.y + half;
    if (top < margin || bottom > limit) {
        if (mEdgeFade > 0.0f) {
            mEdgeFade -= 0.1f;
            if (mEdgeFade < 0.3f) {
                mEdgeFade = 0.3f;
            }
        }
    } else if (mEdgeFade < 1.0f) {
        mEdgeFade += 0.1f;
        if (mEdgeFade > 1.0f) {
            mEdgeFade = 1.0f;
        }
    }
    mAlpha = mAlpha * mEdgeFade;
}

u16 CityLabel::GetWeatherCode(s8 day) {
    if (day < 2) {
        CityForecast* forecast = mCity->GetForecast();
        CitySummary* summary = mCity->GetSummary();
        if (forecast != NULL) {
            return forecast->mEntry->mDays[day].mWeather;
        }
        if (summary != NULL) {
            return summary->mEntry->mDays[day].mWeather;
        }
        return 255;
    }
    if (mCity->mNow != NULL) {
        return mCity->mNow->mEntry->mWeather;
    }
    return 255;
}

Vector2 CityLabel::GetIconSize(s8 day) {
    return Vector2(90.0f, 90.0f);
}

Vector2 CityLabel::GetTempSize(s8 day) {
    if (gSimpleGlobe != NULL && gSimpleGlobe->mZoomLevel > gLabelDetailZoom) {
        return GetTempSizeLarge(day);
    } else {
        return GetTempSizeSmall(day);
    }
}

Vector2 CityLabel::GetTempSizeSmall(s8 day) {
    return Vector2(116.0f, 116.0f);
}

Vector2 CityLabel::GetTempSizeLarge(s8 day) {
    // Day 2 is the current weather
    const wchar_t* text;
    switch (day) {
    case 2:
        text = mNowText[gTempUnit];
        break;
    default:
        text = mDays[day].mMax[gTempUnit];
        break;
    }

    f32 width = CalcTempWidth(text) + 20.0f;
    f32 height = GetTempGlyphHeight(text) + 20.0f;
    return Vector2(width, height);
}
