#include <channel/System.h>
#include <channel/Scene.h>
#include <channel/LayoutObj.h>

#include <revolution/GX.h>
#include <revolution/SC.h>
#include <revolution/VI.h>

// Layout allocator (nw4r::lyt)
extern "C" MEMAllocator* lbl_803311C8;
// Error screen layout archive
extern "C" u8 lbl_8019C6C0[];

// Region group from the console's product area: 0 = Japan/Taiwan, 2 = PAL, 1 = everything else
s32 GetAreaGroup(void) {
    switch (SCGetProductArea()) {
    case SC_AREA_JPN:
    case SC_AREA_TWN:
        return 0;
    case SC_AREA_EUR:
    case SC_AREA_AUS:
    case SC_AREA_SAF:
        return 2;
    default:
        return 1;
    }
}

// System language, falling back to English if it's out of range
u8 GetLanguage(void) {
    u8 lang = SCGetLanguage();

    switch (lang) {
    case SC_LANG_JP:
    case SC_LANG_EN:
    case SC_LANG_DE:
    case SC_LANG_FR:
    case SC_LANG_SP:
    case SC_LANG_IT:
    case SC_LANG_NL:
        return lang;
    default:
        return SC_LANG_EN;
    }
}

void Cursor::Reset() {
    unk20 = 0;
    unk24 = 0;
    unk28 = 0;
    unk2C = 0;
}

FatalScene::FatalScene() {
    lbl_803311C8 = &gMEM1Allocator;
    mLayout = new LayoutObj(lbl_8019C6C0, "error_system.brlyt", 0);
}

FatalScene::~FatalScene() {
    delete mLayout;
}

void FatalScene::Init() {
    mLayout->Reset();
    mExitTimer = 0;
    VISetBlack(FALSE);
    StartFade(FADE_TO_BLACK, 25, FADE_NONE, 0);

    GXColor clear = {0, 0, 0, 255};
    GXSetCopyClear(clear, 0xFFFFFF);
}

void FatalScene::Calc() {
    mLayout->Calc();

    if (mExitTimer > 0) {
        if (--mExitTimer == 0) {
            ReturnToMenu();
        }
    } else if (gTrigAll & WPAD_BUTTON_A) {
        mExitTimer = 1;
        StartFade(FADE_CAPTURE_BRIGHTNESS, 25, FADE_NONE, 0);
    }
}

void FatalScene::Draw() {
    mLayout->Draw();
}
