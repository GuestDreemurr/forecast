// d_weather_a_tomorrow.cpp: tomorrow's forecast panel
#include <channel/WeatherBase.h>
#include <channel/ColorWhite.h>
#include <channel/ForecastData.h>
#include <channel/LayoutButton.h>
#include <channel/SceneBase.h>
#include <channel/System.h>
#include <channel/WeatherScene.h>

extern f32 gUnkSceneFloat;
extern nw4r::ut::Font* gSysFont;
extern nw4r::math::MTX34 gModelMtx;

WeatherTomorrow::WeatherTomorrow(const Vec2& pos, void* arc, const Vec2& size, s32 type)
    : WeatherBaseDay(pos, arc, size, type), mDrawFunc(NULL) {
    if (gLanguage != 0) {
        mBoxes[2].mColor.r = gColorOrange.r;
        mBoxes[2].mColor.g = gColorOrange.g;
        mBoxes[2].mColor.b = gColorOrange.b;
        mBoxes[2].mColor.a = gColorOrange.a;
        mBoxes[3].mColor.r = gColorOrange.r;
        mBoxes[3].mColor.g = gColorOrange.g;
        mBoxes[3].mColor.b = gColorOrange.b;
        mBoxes[3].mColor.a = gColorOrange.a;
    }
}

void WeatherTomorrow::Reset() {
    mLayout->Reset();
}

void WeatherTomorrow::Draw() {
    if (mCulled) {
        return;
    }

    s32 alpha = mScale * mAlpha;
    if (IsEmpty()) {
        mNoServiceText->SetState(3);
        mNoServiceLayout->SetPaneAlpha(alpha);
        mNoServiceLayout->Draw();
        return;
    }

    mLayout->SetPaneAlpha(alpha);
    mLayout->Draw();
    alpha = 255.0f * mScale;
    if (mDrawFunc) {
        (this->*mDrawFunc)(alpha);
    }
}

void WeatherTomorrow::SetPosition(const Vec2& pos, const f32& scale, const bool& visible, BOOL checkHover) {
    WeatherBaseDay::SetPosition(pos, scale, visible, checkHover);
    mLayout->Calc();

    City* city = gCurrentCity;
    mDrawFunc = NULL;
    if (city != NULL) {
        if (city->mForecast != NULL) {
            mDrawFunc = &WeatherTomorrow::DrawForecast;
        } else if (city->mSummary != NULL) {
            mDrawFunc = &WeatherTomorrow::DrawSummary;
        }
    }

    nw4r::math::VEC3 trans(mPos.x, mPos.y, 0.0f);
    PSMTXTrans(gModelMtx, trans.x, trans.y, trans.z);
    mLayout->SetViewMtx(gModelMtx);
    mNoServiceLayout->SetViewMtx(gModelMtx);
}

BOOL WeatherTomorrow::IsEmpty() {
    if (gCurrentCity != NULL) {
        CityForecast* forecast = gCurrentCity->mForecast;
        if (gCurrentCity->mInfo != NULL) {
            if (forecast != NULL) {
                if (forecast->mEntry->mDays[1].mWeather != 0xFFFF) {
                    u32 code = forecast->mEntry->mDays[1].mWeather;
                    if (gForecastData->FindWeatherInfo(code) != NULL) {
                        return FALSE;
                    }
                }
            } else {
                CitySummary* summary = gCurrentCity->mSummary;
                if (summary != NULL && summary->mEntry->mDays[1].mWeather != 0xFFFF) {
                    u32 code = summary->mEntry->mDays[1].mWeather;
                    if (gForecastData->FindWeatherInfo(code) != NULL) {
                        return FALSE;
                    }
                }
            }
        }
    }

    return TRUE;
}

void WeatherTomorrow::DrawForecast(const s32& alpha) {
    CityForecast* forecast = gCurrentCity->mForecast;
    if (gCurrentCity->mInfo != NULL && forecast != NULL) {
        u32 code = forecast->mEntry->mDays[1].mWeather;
        WeatherInfo* info = gForecastData->FindWeatherInfo(code);
        if (info != NULL) {
            f32 scale = 1.0f;
            Vec2F center(mPos.x + 0.5f * GetScreenWidth(), 228.0f - mPos.y);
            SetDefaultGXState();
            SetOrthoProjection();
            mWriter.SetFont(*gSysFont);
            mWriter.SetDrawFlag(0x100);
            mWriter.SetScale(scale);
            mWriter.SetCharSpace(scale * gUnkSceneFloat);
            mWriter.SetTextColor(nw4r::ut::Color(255, 255, 255, alpha));
            mWriter.SetupGX();

            if (gLanguage == 0) {
                DrawDayJP(&forecast->mEntry->mDays[1], info, alpha, scale);
            } else {
                DrawDay(&forecast->mEntry->mDays[1], info, alpha, scale);
            }
        }
    }
}

void WeatherTomorrow::DrawSummary(const s32& alpha) {
    CitySummary* summary = gCurrentCity->mSummary;
    if (gCurrentCity->mInfo != NULL && summary != NULL) {
        u32 code = summary->mEntry->mDays[1].mWeather;
        WeatherInfo* info = gForecastData->FindWeatherInfo(code);
        if (info != NULL) {
            f32 scale = 1.0f;
            Vec2F center(mPos.x + 0.5f * GetScreenWidth(), 228.0f - mPos.y);
            SetDefaultGXState();
            SetOrthoProjection();
            mWriter.SetFont(*gSysFont);
            mWriter.SetDrawFlag(0x100);
            mWriter.SetScale(scale);
            mWriter.SetCharSpace(scale * gUnkSceneFloat);
            mWriter.SetTextColor(nw4r::ut::Color(255, 255, 255, alpha));
            mWriter.SetupGX();

            if (gLanguage == 0) {
                DrawDayJP(&summary->mEntry->mDays[1], info, alpha, scale);
            } else {
                DrawDay(&summary->mEntry->mDays[1], info, alpha, scale);
            }
        }
    }
}
