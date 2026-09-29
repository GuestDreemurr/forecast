// d_scene.cpp: base class of the channel's scenes and the resources they share
#include <channel/SceneBase.h>
#include <channel/HomeButton.h>
#include <channel/System.h>

#include <revolution/VI.h>

extern "C" void ShutdownDownloader(s32 event);

static const u32 sLanguageTextures[] = {100, 102, 98, 102, 101, 99, 97};

HomeButton* gHomeButton;
u8 gExitRequested;
SimpleModel* gEarthModel;

void RequestExit() {
    gExitRequested = TRUE;
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

void SceneBase::unk38() {}

void SceneBase::unk3C() {
    SetCursor(-1, 1);
}

void SceneBase::unk40() {}

void SceneBase::unk28() {}

void SceneBase::unk24() {}

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
