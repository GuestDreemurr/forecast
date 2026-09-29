// d_scene.cpp: base class of the channel's scenes and the resources they share
#include <channel/SceneBase.h>
#include <channel/Fade.h>
#include <channel/HomeButton.h>
#include <channel/System.h>

#include <revolution/VI.h>
#include <nw4r/math.h>

extern "C" void ShutdownDownloader(s32 event);
void UpdateSound();

static const u32 sLanguageTextures[] = {100, 102, 98, 102, 101, 99, 97};

HomeButton* gHomeButton;
u8 gFatalRequested;
SimpleModel* gEarthModel;

void RequestFatal() {
    gFatalRequested = TRUE;
}

void SceneBase::Exit(BOOL shutdownNet, s32 event) {
    VISetBlack(TRUE);
    VIFlush();
    VIWaitForRetrace();
    StopRumble(-1, 0);

    if (shutdownNet) {
        ShutdownDownloader(event);
    }
}

void SceneBase::unk20() {
    if (gSound != NULL) {
        UpdateSound();
    }
}

void SceneBase::unk2C() {
    if (gEarthModel != NULL) {
        gGlobeAlpha -= 6;
        if (gGlobeAlpha < 0) {
            gGlobeAlpha = 0;
        }
    } else {
        gGlobeAlpha = 255;
    }
}

void SceneBase::Draw() {
    if (!gFatalRequested) {
        (this->*mDrawFunc)();

        if (!gHomeButton->IsOpen()) {
            unk3C();
        }
    }
}

void SceneBase::DrawOverlay() {
    if (!gFatalRequested) {
        if (gFade != NULL) {
            gFade->Draw();
        }
        if (gFade2 != NULL) {
            gFade2->Draw();
        }

        unk38();
        gHomeButton->Draw();
    }
}

void SceneBase::unk38() {}

void SceneBase::unk3C() {
    SetCursor(-1, 1);
}

void SceneBase::unk40() {}

void SceneBase::UpdateMenuFade() {
    if (gPointerOverMenu || gMenuVisible) {
        if (mMenuShadeAlpha != 0) {
            mMenuShadeAlpha -= 20;
            if (mMenuShadeAlpha < 0) {
                mMenuShadeAlpha = 0;
            }
        }

        gMenuBrightness += 0.1f;
        if (gMenuBrightness > 1.0f) {
            gMenuBrightness = 1.0f;
        }
    } else {
        if (mMenuShadeAlpha != 255) {
            mMenuShadeAlpha += 20;
            if (mMenuShadeAlpha > 255) {
                mMenuShadeAlpha = 255;
            }
        }

        gMenuBrightness -= 0.1f;
        if (gMenuBrightness < 0.2f) {
            gMenuBrightness = 0.2f;
        }
    }
}

void SceneBase::unk28() {}

void SceneBase::unk24() {}

BOOL SceneBase::StateFatal() {
    switch (mStatePhase) {
    case 0:
        mStatePhase++;
        GXColor clear = {0, 0, 0, 0};
        GXSetCopyClear(clear, 0xFFFFFF);
        StartFade(FADE_CAPTURE_BRIGHTNESS, 20, FADE_NONE, 0);
        break;
    case -1:
        break;
    default:
        gNextScene = SCENE_FATAL;
        break;
    }

    return TRUE;
}

void ToDegrees(u16 lon, u16 lat, Vec2* out) {
    out->y = (f32)lat * (360.0f / 65536.0f);
    out->x = (f32)(s16)lon * (360.0f / 65536.0f);
}

u32 GetLanguageTexture() {
    return sLanguageTextures[gLanguage];
}

void SceneBase::OnShutdown() {
    Exit(TRUE, 2);
}

void SceneBase::OnPowerButton() {
    if (gHomeButton != NULL) {
        gHomeButton->OnReset();
    }
}

void SceneBase::OnResetButton() {
    if (gHomeButton != NULL) {
        gHomeButton->OnReset();
    }
}

void SceneBase::Init() {}
