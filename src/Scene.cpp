#include <channel/System.h>
#include <channel/Scene.h>
#include <channel/LayoutObj.h>

#include <revolution/GX.h>
#include <revolution/SC.h>
#include <revolution/VI.h>
#include <revolution/OS.h>

// Layout allocator (nw4r::lyt)
extern "C" MEMAllocator* lbl_803311C8;
// Error screen layout archive
extern "C" u8 lbl_8019C6C0[];
// Cursor textures
extern "C" TPLPalette* lbl_80330C00;

namespace nw4r {
namespace ef {
class Resource {
public:
    bool Add(u8* data);
    bool AddTexture(u8* data);
    void RelocateCommand();
};
}
}

typedef struct {
    f32 m[3][4];
} EffectMtx;

// Effect draw settings (built on the stack each frame)
struct EffectDrawInfo {
    EffectDrawInfo() {
        PSMTXIdentity(mViewMtx.m);
        PSMTXIdentity(mMtx2.m);
        unk60 = 0;
        unk64 = 0;
        unk68 = 1;
        unk6C = 0;
        unk70 = 0.0f;
        unk74 = 1.0f;
        unk78 = 0.0f;
        unk7C = 1.0f;
    }

    EffectMtx mViewMtx; // at 0x0
    EffectMtx mMtx2;    // at 0x30
    u8 unk60;           // at 0x60
    u32 unk64;          // at 0x64
    u8 unk68;           // at 0x68
    u32 unk6C;          // at 0x6C
    f32 unk70;          // at 0x70
    f32 unk74;          // at 0x74
    f32 unk78;          // at 0x78
    f32 unk7C;          // at 0x7C
    u32 unk80;          // at 0x80
};

struct EffectSystem {
    void* mMemoryManager; // at 0x0
};

// Not yet decompiled (nw4r::ef)
extern "C" void* fn_80093A40(void* manager, void* heap, u32 heapSize, s32 arg3, s32 arg4, s32 arg5, s32 arg6);
extern "C" EffectSystem* fn_800856D0(void);
extern "C" void fn_800856DC(EffectSystem* system, s32 arg);
extern "C" nw4r::ef::Resource* fn_80092674(void);
extern "C" void fn_80085ADC(EffectSystem* system, const EffectDrawInfo* info, s32 arg);

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

Cursor::Cursor() {
    void* memory;
    EffectSystem* system;
    nw4r::ef::Resource* resource;
    u32 texSize;

    mEffectReady = FALSE;
    mEffectHeap = MEM2Alloc(0x20000, 32);

    memory = operator new(0x4C);
    if (memory != NULL) {
        memory = fn_80093A40(memory, mEffectHeap, 0x20000, 32, 64, 64, 64);
    }
    mEffectMemory = memory;

    system = fn_800856D0();
    system->mMemoryManager = memory;
    if (memory != NULL) {
        fn_800856DC(system, 1);
    }

    mEffectData = NULL;
    mEffectTexData = NULL;
    resource = fn_80092674();
    mEffectData = LoadCompressedContentFile(gUnk80330B64, "nw4r_defcursor_all01.breff.LZ", 32, NULL, gMEM2Heap);
    mEffectTexData = LoadCompressedContentFile(gUnk80330B64, "nw4r_defcursor_all01.breft.LZ", 32, &texSize, gMEM2Heap);

    if (mEffectData != NULL && mEffectTexData != NULL) {
        mEffectReady = TRUE;
        resource->Add((u8*)mEffectData);
        resource->AddTexture((u8*)mEffectTexData);
        DCFlushRange(mEffectTexData, texSize);
        resource->RelocateCommand();
    }

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mType[i] = 0;
        unk30[i] = 0;
        unk40[i] = 0;
        unk50[i] = 0;
        unk10[i] = 0;
    }
}

void Cursor::Reset() {
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mType[i] = 0;
    }
}

void Cursor::Draw() {
    EffectDrawInfo info;
    EffectMtx view;
    Mtx44 proj;
    f32 width = GetScreenWidth();

    // Effects are drawn with Y pointing up
    C_MTXOrtho(proj, 456.0f, 0.0f, 0.0f, width, -100.0f, 100.0f);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
    SetDefaultGXState();

    PSMTXIdentity(view.m);
    info.mViewMtx = view;
    fn_80085ADC(fn_800856D0(), &info, 0);
}

void Cursor::Set(s32 chan, s32 type) {
    if (chan < 0) {
        for (s32 i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
            Set(i, type);
        }
        return;
    }

    BOOL active = FALSE;
    if (gPointerValid[chan][0] && gKPADLatest[chan] >= 0) {
        active = TRUE;
    }
    if (!active) {
        return;
    }

    mType[chan] = type;
    if (type != 5) {
        return;
    }

    Vec scale;
    Vec offset;
    Vec shadowPos;
    Vec pos;
    Mtx mtx;

    scale.x = 1.0f;
    scale.y = 1.0f;
    scale.z = 1.0f;
    Vec2 rotation = gHorizon[chan];
    offset.x = -32.0f;
    offset.y = -32.0f;
    offset.z = 0.0f;

    SetDefaultGXState();
    SetOrthoProjection();
    GXSetCurrentMtx(GX_PNMTX1);

    shadowPos.y = gPointerY[chan][0] + 3.0f;
    shadowPos.x = gPointerX[chan][0] + 3.0f;
    shadowPos.z = 0.0f;
    MakeTransformMtx(&scale, &rotation, &shadowPos, mtx);
    GXLoadPosMtxImm(mtx, GX_PNMTX1);
    GXColor shadowColor = {0, 0, 0, 255};
    GXSetTevColor(GX_TEVREG0, shadowColor);
    DrawTextureAt(lbl_80330C00, 2, 1.0f, 1.0f, &offset);

    pos.x = gPointerX[chan][0];
    pos.y = gPointerY[chan][0];
    pos.z = 0.0f;
    MakeTransformMtx(&scale, &rotation, &pos, mtx);
    GXLoadPosMtxImm(mtx, GX_PNMTX1);
    GXColor color = {255, 255, 255, 255};
    GXSetTevColor(GX_TEVREG0, color);
    DrawTextureAt(lbl_80330C00, 1, 1.0f, 1.0f, &offset);
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
