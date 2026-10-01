// Connect.cpp: downloads the forecast through WiiConnect24 and shows the "connecting" and error screens
#include <channel/WeatherViews.h>
#include <channel/WeatherScene.h>
#include <channel/Fade.h>
#include <channel/ForecastData.h>
#include <channel/LayoutButton.h>
#include <channel/SceneBase.h>
#include <channel/Sound.h>
#include <channel/System.h>
#include <channel/WiiConnect24.h>

#include <nw4r/math.h>
#include <revolution/GX.h>
#include <revolution/OS.h>
#include <cstdio>
#include <cstdlib>
#include <wstring.h>

static const s32 sProgressIcons[2][6] = {
    {0, 1, 2, 3, 4, 5},
    {15, 15, 15, 15, 15, 15},
};

Connect::Connect(void* arc, ForecastData* data) {
    mData = data;
    mForecastBuf = MEM2Alloc(0x32000, 0);
    mShortBuf = MEM2Alloc(0x5000, 0);
    mFade = gFade;
    mConnectLayout = new ButtonGroup(arc, "error1.brlyt", gButtonColors, false);
    mErrorLayout = new ButtonGroup(arc, "error0.brlyt", gButtonColors, false);
    Start();
}

Connect::~Connect() {
    delete mErrorLayout;
    delete mConnectLayout;
    MEM2Free(mShortBuf);
    MEM2Free(mForecastBuf);
}

void Connect::Start() {
    mState = 0;
    mTask = -1;
    mForecastTime = 0;
    mShortTime = 0;
    mTaskState = 999;
    mTaskResult = 999;
    mForecastCheck = 0;
    mShortCheck = 0;
    mServerMessage = NULL;
    mProgressFrame = 0;
    mDisplayState = 0;
    mSoundPlaying = FALSE;

    u8 language = gLanguage;
    u32 country = gCountryCode >> 24;
    sprintf(mForecastUrl, "http://weather.wapp.wii.com/%d/%03d/forecast.bin", language, country);
    sprintf(mShortUrl, "http://weather.wapp.wii.com/%d/%03d/short.bin", language, country);

    mConnectLayout->Reset();
    mErrorLayout->Reset();
    LayoutButton* button = mErrorLayout->FindButton("next");
    button->mPressed = TRUE;
}

static inline wchar_t* GetServerMessage(void* data, const u32& size) {
    u8* buf = (u8*)data;
    u32 offset = *(u32*)(buf + 0x1C);
    if (offset == 0) {
        return NULL;
    }

    u8* end = buf + size;
    wchar_t* message = (wchar_t*)(buf + offset);
    wchar_t* p = message;
    for (int i = 0; (u8*)p < end && i < 0x200; p++, i++) {
        if (*p == 0) {
            return message;
        }
    }

    return NULL;
}

void Connect::Calc() {
    s32 prevState = mState;

    mConnectLayout->Calc();
    mErrorLayout->Calc();

    switch (mState) {
    case 0: {
        BOOL reset = FALSE;
        for (int i = 0; i < 4; i++) {
            if ((gHold[i] & 0x1310) == 0x1310) {
                reset = TRUE;
                break;
            }
        }

        if (reset) {
            mTask = CWiiConnect24::deleteWeather();
            mState = 1;
        } else {
            mTask = CWiiConnect24::getWeather(mForecastBuf, &mForecastTime, &mForecastSize, mShortBuf, &mShortTime,
                                              &mShortSize, mForecastUrl, mShortUrl, 0x32000, 0x32000, 0x5000);
            if (mTask < 0) {
                OSPanic("Connect.cpp", 163, "CWiiConnect24::getWeather() failed.\n");
            }
            mState = 2;
        }
        break;
    }
    case 1:
        mTaskState = gWC24Tasks[mTask].mState;
        break;
    case 2: {
        WC24Task* task = &gWC24Tasks[mTask];
        mTaskState = task->mState;
        if (mTaskState == 0) {
            mTaskResult = task->mResult;
            switch (mTaskResult) {
            case 0:
                    OSGetTick();
                    if (CheckForecastData(mForecastBuf, mForecastSize, &mForecastCheck, mShortBuf, mShortSize,
                                          &mShortCheck)) {
                        mTask = CWiiConnect24::setWeather(mForecastUrl, mShortUrl, 0x32000, 0, 0);
                        if (mTask < 0) {
                            OSPanic("Connect.cpp", 196, "CWiiConnect24::setWeather() failed.\n");
                        }
                        mState = 5;
                    } else if (mForecastCheck == -3 || mShortCheck == -3) {
                        mState = 7;
                    } else {
                        mTask = CWiiConnect24::downloadWeatherForecast(mForecastBuf, &mForecastTime, &mForecastSize,
                                                                       0x32000);
                        if (mTask < 0) {
                            OSPanic("Connect.cpp", 211, "CWiiConnect24::downloadWeatherForecast");
                        }
                        mState = 3;
                    }
                break;
            default:
                mState = 7;
                break;
            }
        }
        break;
    }
    case 3: {
        WC24Task* task = &gWC24Tasks[mTask];
        mTaskState = task->mState;
        if (mTaskState == 0) {
            mTaskResult = task->mResult;
            switch (mTaskResult) {
            case 0:
                    mTask = CWiiConnect24::downloadWeatherShort(mShortBuf, &mShortTime, &mShortSize, 0x5000);
                    if (mTask < 0) {
                        OSPanic("Connect.cpp", 237, "CWiiConnect24::downloadWeatherShort");
                    }
                    mState = 4;
                break;
            default:
                mState = 7;
                break;
            }
        }
        break;
    }
    case 4: {
        WC24Task* task = &gWC24Tasks[mTask];
        mTaskState = task->mState;
        if (mTaskState == 0) {
            mTaskResult = task->mResult;
            switch (mTaskResult) {
            case 0:
                    OSGetTick();
                    if (CheckForecastData(mForecastBuf, mForecastSize, &mForecastCheck, mShortBuf, mShortSize,
                                          &mShortCheck)) {
                        mTask = CWiiConnect24::setWeather(mForecastUrl, mShortUrl, 0x32000, 0, 0);
                        if (mTask < 0) {
                            OSPanic("Connect.cpp", 269, "CWiiConnect24::setWeather() failed.\n");
                        }
                        mState = 5;
                    } else {
                        mState = 7;
                    }
                break;
            default:
                mState = 7;
                break;
            }
        }
        break;
    }
    case 5: {
        WC24Task* task = &gWC24Tasks[mTask];
        mTaskState = task->mState;
        if (mTaskState == 0) {
            mTaskResult = task->mResult;
            switch (mTaskResult) {
            case 0:
                    mState = 6;
                break;
            default:
                mState = 7;
                break;
            }
        }
        break;
    }
    case 6:
    case 7:
        break;
    }

    if (mState == 6 && prevState != mState) {
        mServerMessage = GetServerMessage(mForecastBuf, mForecastSize);
        if (mServerMessage != NULL) {
            mState = 7;
        }

        if (mState != 7 && mData->LoadForecast(mForecastBuf) != 24) {
            mState = 7;
        }

        if (mState != 7 && mData->LoadShort(mShortBuf) != 24) {
            mState = 7;
        }
    }

    switch (mDisplayState) {
    case 0:
        mDisplayState = 1;
        break;
    case 1:
        if (mState == 6) {
            mDisplayState = 6;
        } else if (mState == 1) {
            if (mTaskState == 0) {
                gReturnToMenuRequested = TRUE;
            }
        } else if (mState == 7) {
            mFade->FadeIn(30);
            mDisplayState = 5;
            PlaySE(25);
        } else if (mTaskState == 4 || mTaskState == 5) {
            mFade->FadeIn(30);
            mDisplayState = 2;
        }
        break;
    case 2:
        if (mFade->mFading == 0) {
            if (mState == 6) {
                mFade->FadeOut(50);
                mDisplayState = 4;
                PlaySE(24);
            } else if (mState == 7) {
                mFade->FadeOut(30);
                mDisplayState = 3;
            }
        }
        break;
    case 3:
        if (mFade->mFading == 0) {
            mFade->FadeIn(30);
            mDisplayState = 5;
            PlaySE(25);
        }
        break;
    case 4:
        if (mFade->mFading == 0) {
            mDisplayState = 6;
        }
        break;
    case 5:
        UpdateButtons(mErrorLayout, 40);
        if (mFade->mFading == 0 && CheckButtonPressed("next", WPAD_BUTTON_A) >= 0) {
            PlaySE(26);
            gReturnToMenuRequested = TRUE;
        }
        break;
    case 6:
        break;
    }

    switch (mDisplayState) {
    case 2:
    case 3:
    case 4:
        if (++mProgressFrame > 72) {
            mProgressFrame = 0;
        }
        break;
    }

    if (mDisplayState == 2) {
        if (!mSoundPlaying) {
            StartSound(&mSound, 23);
            mSoundPlaying = TRUE;
        }

        if (mSoundPlaying && IsSoundPaused(&mSound)) {
            ::PauseSound(&mSound, false, 0);
        }
    } else if (mSoundPlaying) {
        StopSound(&mSound, 0);
        mSoundPlaying = FALSE;
    }
}

void Connect::Draw() {
    switch (mDisplayState) {
    case 2:
    case 3:
    case 4:
        mConnectLayout->Draw();
        DrawProgress();
        break;
    case 5: {
        LayoutButton* text;
        s32 message = 0;
        text = mErrorLayout->FindButton("text");
        switch (mTaskResult) {
        case -11:
            text->SetState(0);
            break;
        case -3:
            text->SetState(1);
            break;
        case -9:
        case -4:
            text->SetState(2);
            break;
        case -5:
            text->SetState(3);
            break;
        case -12:
            text->SetState(6);
            break;
        case -2:
            text->SetState(7);
            break;
        case 0:
            if (mServerMessage != NULL) {
                LayoutButton* server = mErrorLayout->FindButton("error_server");
                server->SetText(mServerMessage);
                text->SetState(-1);
            } else {
                message = 6;
            }
            break;
        case -8:
            if (gWC24Tasks[mTask].mNwc24Result == -4) {
                text->SetState(4);
            } else {
                message = 1;
            }
            break;
        case -1:
            message = 5;
            break;
        case -6:
            message = 2;
            break;
        case -7:
            message = 3;
            break;
        default:
            message = 99;
            break;
        }

        if (message != 0) {
            text->SetState(5);
        }

        SetErrorCode(gWC24Tasks[mTask].mErrorCode, message);
        mErrorLayout->Draw();
        break;
    }
    }
}

BOOL Connect::IsDone() {
    BOOL done = FALSE;
    if (mState == 6 && mFade->mFading == 0) {
        done = TRUE;
    }
    return done;
}

void Connect::PauseSound(bool pause) {
    if (mSoundPlaying && pause != IsSoundPaused(&mSound)) {
        ::PauseSound(&mSound, pause, 0);
    }
}

static inline void SetProgressColor(u8 alpha) {
    GXColor color = {255, 255, 255, alpha};
    GXSetTevColor(GX_TEVREG0, color);
}

void Connect::DrawProgress() {
    SetDefaultGXState();
    SetOrthoProjection();

    f32 x = 0.5f * (GetScreenWidth() - 200.0f);
    s32 current = mProgressFrame / 12;
    s32 frame = mProgressFrame % 12;
    for (int i = 0; i < 6; i++) {
        u8 alpha;
        if (i == current) {
            f32 rad = 1.5707964f * (frame + 1) / 12.0f;
            alpha = 255.0f * nw4r::math::SinRad(rad);
        } else {
            s32 behind = current - i;
            if (behind < 0) {
                behind += 6;
            }

            s32 t = 72 - (frame + behind * 12);
            if (t < 0) {
                t = 0;
            }

            alpha = 255.0f * t / 72.0f;
        }

        SetProgressColor(alpha);

        u32 icon = sProgressIcons[gRegion != 0][i];
        f32 y = 280.0f - 0.5f * (0.6f * GetTexHeight((TPLPalette*)gUnk80330B74, icon));
        Vec pos;
        pos.x = x - 0.5f * (0.6f * GetTexWidth((TPLPalette*)gUnk80330B74, icon));
        pos.y = y;
        pos.z = 0.0f;
        DrawTextureAt((TPLPalette*)gUnk80330B74, icon, 0.6f, 0.6f, &pos);
        x += 40.0f;
    }
}

void Connect::SetErrorCode(s32 wc24Code, s32 localCode) {
    s32 code;
    const wchar_t* prefix;
    LayoutButton* button;
    const wchar_t* label;

    if (wc24Code != 0) {
        code = wc24Code;
        prefix = L"";
    } else if (localCode != 0) {
        code = localCode;
        prefix = L"FORE";
    } else {
        return;
    }

    if (code < 0) {
        code = -code;
    }
    switch (gLanguage) {
    case 0:
        label = L"\x30A8\x30E9\x30FC\x30B3\x30FC\x30C9\xFF1A";
        break;
    case 1:
        label = L"Error Code:";
        break;
    case 2:
        label = L"Fehlercode:";
        break;
    case 3:
        label = L"Code d'erreur:";
        break;
    case 4:
        label = L"Error:";
        break;
    case 5:
        label = L"Codice errore:";
        break;
    case 6:
        label = L"Fout:";
        break;
    }

    button = mErrorLayout->FindButton("error_code");
    wchar_t buf[0x80];
    swprintf(buf, 0x80, L"%ls %ls%06d", label, prefix, code);
    button->SetText(buf);
}
