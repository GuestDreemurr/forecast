// d_weather.cpp: the current weather panel, from short.bin
#include <channel/WeatherBase.h>
#include <channel/ColorWhite.h>
#include <channel/ForecastData.h>
#include <channel/LayoutButton.h>
#include <channel/SceneBase.h>
#include <channel/System.h>
#include <channel/WeatherScene.h>

#include <revolution/GX.h>

extern f32 gUnkSceneFloat;
extern wchar_t sTextBuf[0x100];
extern nw4r::ut::Font* gSysFont;
extern nw4r::math::MTX34 gModelMtx;

void DrawWeatherIcon(u16 icon, const Vec2* pos, f32 scale, s32 alpha);

WeatherNow::WeatherNow(const Vec2& pos, void* arc, const Vec2& size, s32 type)
    : WeatherBaseDay(pos, arc, size, type) {}

WeatherNow::~WeatherNow() {}

void WeatherNow::Reset() {
    mLayout->Reset();
}

void WeatherNow::Draw() {
    if (mCulled) {
        return;
    }

    s32 alpha = mScale * mAlpha;
    if (IsEmpty()) {
        mNoServiceText->SetState(1);
        mNoServiceLayout->SetPaneAlpha(alpha);
        mNoServiceLayout->Draw();
        return;
    }

    mLayout->SetPaneAlpha(alpha);
    mLayout->Draw();
    alpha = 255.0f * mScale;
    DrawNow(alpha);
}

void WeatherNow::SetPosition(const Vec2& pos, const f32& scale, const bool& visible, BOOL checkHover) {
    WeatherBaseDay::SetPosition(pos, scale, visible, FALSE);
    mLayout->Calc();
    nw4r::math::VEC3 trans(mPos.x, mPos.y, 0.0f);
    PSMTXTrans(gModelMtx, trans.x, trans.y, trans.z);
    mLayout->SetViewMtx(gModelMtx);
    mNoServiceLayout->SetViewMtx(gModelMtx);
}

BOOL WeatherNow::IsEmpty() {
    if (gCurrentCity != NULL) {
        CityNow* now = gCurrentCity->mNow;
        CityInfo* info = gCurrentCity->mInfo;
        if (now != NULL && info != NULL) {
            u32 code = now->mEntry->mWeather;
            WeatherInfo* weather = gForecastData->FindWeatherInfo(code);
            if (now->mEntry->mWeather != 0xFFFF && weather != NULL) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

void WeatherNow::DrawNow(const s32& alpha) {
    CityNow* now = gCurrentCity->mNow;
    if (gCurrentCity->mInfo == NULL) {
        return;
    }

    u32 code = now->mEntry->mWeather;
    WeatherInfo* info = gForecastData->FindWeatherInfo(code);
    if (info == NULL) {
        return;
    }

    f32 scale = 1.0f;
    Vec2F pos;
    Vec2F center(mPos.x + 0.5f * GetScreenWidth(), 228.0f - mPos.y);
    pos.x = mBoxes[4].mX + mPos.x;
    pos.y = mBoxes[4].mY - mPos.y;
    DrawWeatherIcon(info->mType->mIcon, &pos, scale * mBoxes[4].mScaleX, alpha);

    SetDefaultGXState();
    SetOrthoProjection();
    mWriter.SetFont(*gSysFont);
    mWriter.SetDrawFlag(0x111);
    mWriter.SetScale(scale);
    mWriter.SetCharSpace(scale * gUnkSceneFloat);
    mWriter.SetupGX();
    DrawText(&mBoxes[0], info->mText, alpha);

    u8 dir = now->mEntry->mWindDirection;
    if (dir != 0xFF) {
        u8 speed = gWindUnit == 0 ? now->mEntry->mWindSpeedMph : now->mEntry->mWindSpeedKmh;
        FormatWind(dir, speed);
        DrawTextFit(&mBoxes[1], sTextBuf, alpha);
    }

    SetDefaultGXState();
    SetOrthoProjection();
    GXSetTevColorIn(GX_TEVSTAGE0, (GXTevColorArg)4, (GXTevColorArg)8, (GXTevColorArg)2, (GXTevColorArg)4);
    if (gTempUnit == 0) {
        DrawTempC(now->mEntry->mTempC, alpha);
    } else {
        DrawTemp(now->mEntry->mTempF, alpha);
    }
}
