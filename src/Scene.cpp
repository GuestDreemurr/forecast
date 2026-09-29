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
extern "C" void fn_80085150(void* emitterSet);
extern "C" void* fn_80085868(EffectSystem* system, const char* name, s32 arg2, s32 arg3);
extern "C" void fn_80085B54(EffectSystem* system, f32 arg1, f32 arg2, const Vec* arg3, const Mtx arg4);
extern "C" void fn_80085A4C(EffectSystem* system, s32 arg1, s32 arg2);
extern "C" u16 fn_80085634(void* emitterSet);
extern "C" void* fn_8008563C(void* emitterSet, u16 idx);
extern "C" u16 fn_80087AA8(void* emitter);
extern "C" void* fn_80087AB0(void* emitter, u16 idx);
extern "C" void* fn_800CFC20(void* list, void* obj);

struct EmitterTransform {
    u8 unk0[0x8C];  // at 0x0
    Vec mTranslate; // at 0x8C
};
extern "C" EmitterTransform* fn_8008562C(void* emitterSet);
extern "C" void fn_800879BC(EmitterTransform* transform);

namespace nw4r {
namespace math {
f32 Atan2FIdx(f32 y, f32 x);
f32 CosFIdx(f32 fidx);
}
}

struct Particle {
    u8 unk0[0xC];          // at 0x0
    s32 mState;            // at 0xC
    u8 unk10[0x10];        // at 0x10
    GXColor mColor[2][2];  // at 0x20
    u8 unk30[0x18];        // at 0x30
    f32 mRotation;         // at 0x48
};

// Cursor type -> effect style (normal, hold, open); type 5 draws a plain texture instead
static const s32 sCursorStyle[] = {0, 0, 1, 2, 2, -1};
// Starting spin counter per controller
static const s32 sSpinStart[WPAD_MAX_CONTROLLERS] = {0, 4, 2, 6};

static const char* sShadowEffects[] = {
    "def_cursor_normal_sd",
    "def_cursor_hold_sd",
    "def_cursor_open_sd",
};
static const char* sCursorEffects[][WPAD_MAX_CONTROLLERS] = {
    {"def_cursor_normal_1p", "def_cursor_normal_2p", "def_cursor_normal_3p", "def_cursor_normal_4p"},
    {"def_cursor_hold_1p", "def_cursor_hold_2p", "def_cursor_hold_3p", "def_cursor_hold_4p"},
    {"def_cursor_open_1p", "def_cursor_open_2p", "def_cursor_open_3p", "def_cursor_open_4p"},
};

static inline BOOL IsPointerActive(s32 chan) {
    BOOL active = FALSE;
    if (gPointerValid[chan][0] && gKPADLatest[chan] >= 0) {
        active = TRUE;
    }
    return active;
}

static inline f32 Atan2Rad(f32 y, f32 x) {
    return 0.024543693f * nw4r::math::Atan2FIdx(y, x);
}

static inline void* CreateEffect(const char* name) {
    return fn_80085868(fn_800856D0(), name, 0, 0);
}

static inline void SetEmitterTranslate(void* emitterSet, const Vec& pos) {
    EmitterTransform* transform = fn_8008562C(emitterSet);
    transform->mTranslate.x = pos.x;
    transform->mTranslate.y = pos.y;
    transform->mTranslate.z = pos.z;
    fn_800879BC(transform);
}

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
        mCursorEmitter[i] = NULL;
        mSpinEmitter[i] = NULL;
        mShadowEmitter[i] = NULL;
        mSpinCounter[i] = 0;
    }
}

void Cursor::Reset() {
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mType[i] = 0;
    }
}

void Cursor::Calc() {
    Mtx mtx;
    Vec zero;
    f32 rotation[WPAD_MAX_CONTROLLERS];
    Vec shadowPos;
    Vec spinPos;
    Vec cursorPos;
    EffectSystem* system = fn_800856D0();

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        if (mCursorEmitter[i] != NULL) {
            fn_80085150(mCursorEmitter[i]);
            mCursorEmitter[i] = NULL;
        }
        if (mSpinEmitter[i] != NULL) {
            fn_80085150(mSpinEmitter[i]);
            mSpinEmitter[i] = NULL;
        }
        if (mShadowEmitter[i] != NULL) {
            fn_80085150(mShadowEmitter[i]);
            mShadowEmitter[i] = NULL;
        }
    }

    for (int chan = WPAD_MAX_CONTROLLERS - 1; chan >= 0; chan--) {
        if (mType[chan] != 0 && sCursorStyle[mType[chan]] >= 0 && IsPointerActive(chan)) {
            f32 offsetX;
            f32 offsetY;

            rotation[chan] = Atan2Rad(gHorizon[chan].y, gHorizon[chan].x);

            if (mType[chan] == 4) {
                offsetY = ++mSpinCounter[chan] & 7;
                rotation[chan] += 0.7853982f;
                offsetX = offsetY * nw4r::math::CosFIdx(40.743664f * rotation[chan]);
            } else {
                offsetX = 0.0f;
                offsetY = offsetX;
                mSpinCounter[chan] = sSpinStart[chan];
            }

            mShadowEmitter[chan] = CreateEffect(sShadowEffects[sCursorStyle[mType[chan]]]);
            if (mShadowEmitter[chan] != NULL) {
                f32 pointerY = gPointerY[chan][0];
                f32 pointerX = gPointerX[chan][0];
                shadowPos.y = ((456.0f - pointerY) - 3.0f) - offsetY;
                shadowPos.x = offsetX + (3.0f + pointerX);
                shadowPos.z = 0.0f;
                SetEmitterTranslate(mShadowEmitter[chan], shadowPos);
            }

            if (mType[chan] == 4) {
                mSpinEmitter[chan] = CreateEffect(sCursorEffects[2][chan]);
                if (mSpinEmitter[chan] != NULL) {
                    spinPos.x = gPointerX[chan][0] - offsetX;
                    spinPos.y = offsetY + (456.0f - gPointerY[chan][0]);
                    spinPos.z = 0.0f;
                    SetEmitterTranslate(mSpinEmitter[chan], spinPos);
                }
            }

            mCursorEmitter[chan] = CreateEffect(sCursorEffects[sCursorStyle[mType[chan]]][chan]);
            if (mCursorEmitter[chan] != NULL) {
                cursorPos.x = offsetX + gPointerX[chan][0];
                cursorPos.y = (456.0f - gPointerY[chan][0]) - offsetY;
                cursorPos.z = 0.0f;
                SetEmitterTranslate(mCursorEmitter[chan], cursorPos);
            }
        }
        mType[chan] = 0;
    }

    zero.x = 0.0f;
    zero.y = 0.0f;
    zero.z = 0.0f;
    PSMTXIdentity(mtx);
    fn_80085B54(system, -100.0f, 100.0f, &zero, mtx);
    fn_80085A4C(system, 0, 0);

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        if (mCursorEmitter[i] != NULL) {
            UpdateParticles(mCursorEmitter[i], rotation[i], 1.0f);
        }
        if (mSpinEmitter[i] != NULL) {
            UpdateParticles(mSpinEmitter[i], rotation[i], 0.85f);
        }
        if (mShadowEmitter[i] != NULL) {
            UpdateParticles(mShadowEmitter[i], rotation[i], 1.0f);
        }
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

    f32 shadowY = gPointerY[chan][0];
    f32 shadowX = gPointerX[chan][0];
    shadowPos.x = shadowX + 3.0f;
    shadowPos.y = shadowY + 3.0f;
    shadowPos.z = 0.0f;
    MakeTransformMtx(&scale, &rotation, &shadowPos, mtx);
    GXLoadPosMtxImm(mtx, GX_PNMTX1);
    GXColor shadowColor = {0, 0, 0, 255};
    GXSetTevColor(GX_TEVREG0, shadowColor);
    DrawTextureAt(lbl_80330C00, 2, 1.0f, 1.0f, &offset);

    f32 cursorY = gPointerY[chan][0];
    f32 cursorX = gPointerX[chan][0];
    pos.x = cursorX;
    pos.y = cursorY;
    pos.z = 0.0f;
    MakeTransformMtx(&scale, &rotation, &pos, mtx);
    GXLoadPosMtxImm(mtx, GX_PNMTX1);
    GXColor color = {255, 255, 255, 255};
    GXSetTevColor(GX_TEVREG0, color);
    DrawTextureAt(lbl_80330C00, 1, 1.0f, 1.0f, &offset);
}

// Sets rotation on all live particles of an effect and scales their colors
void Cursor::UpdateParticles(void* emitterSet, f32 rotation, f32 brightness) {
    for (u16 i = 0; i < fn_80085634(emitterSet); i++) {
        void* emitter = fn_8008563C(emitterSet, i);

        for (u16 j = 0; j < fn_80087AA8(emitter); j++) {
            void* list = (u8*)fn_80087AB0(emitter, j) + 0x38;
            Particle* particle = NULL;

            while ((particle = (Particle*)fn_800CFC20(list, particle)) != NULL) {
                if (particle->mState != 1 && particle->mState != 2) {
                    continue;
                }

                particle->mRotation = rotation;

                GXColor* color = particle->mColor[0];
                for (int k = 0; k < 2; k++) {
                    color[0].r = color[0].r * brightness;
                    color[0].g = color[0].g * brightness;
                    color[0].b = color[0].b * brightness;
                    color[1].r = color[1].r * brightness;
                    color[1].g = color[1].g * brightness;
                    color[1].b = color[1].b * brightness;
                    color += 2;
                }
            }
        }
    }
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
