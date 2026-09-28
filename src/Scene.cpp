#include <channel/System.h>
#include <channel/Scene.h>

#include <revolution/GX.h>
#include <revolution/SC.h>
#include <revolution/VI.h>

// Not yet decompiled (layout helper, 0x12C bytes)
extern "C" void* fn_8002AF90(void* layout, const void* archive, const char* name, s32 arg);
extern "C" void fn_8002B214(void* layout, s32 del);
extern "C" void fn_8002B3AC(void* layout);
extern "C" void fn_8002B418(void* layout);
extern "C" void fn_8002B604(void* layout);

// Layout allocator (nw4r::lyt)
extern "C" MEMAllocator* lbl_803311C8;
// Error screen layout archive
extern "C" u8 lbl_8019C6C0[];

// Region group from the console's product area: 0 = Japan/Taiwan, 2 = PAL, 1 = everything else
extern "C" s32 fn_8002C0CC(void) {
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
extern "C" u8 fn_8002C124(void) {
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
    void* layout;

    lbl_803311C8 = &gMEM1Allocator;
    layout = operator new(0x12C);
    if (layout != NULL) {
        layout = fn_8002AF90(layout, lbl_8019C6C0, "error_system.brlyt", 0);
    }
    mLayout = layout;
}

FatalScene::~FatalScene() {
    fn_8002B214(mLayout, 1);
}

void FatalScene::Init() {
    fn_8002B3AC(mLayout);
    mExitTimer = 0;
    VISetBlack(FALSE);
    StartFade(FADE_TO_BLACK, 25, FADE_NONE, 0);

    GXColor clear = {0, 0, 0, 255};
    GXSetCopyClear(clear, 0xFFFFFF);
}

void FatalScene::Calc() {
    fn_8002B418(mLayout);

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
    fn_8002B604(mLayout);
}
