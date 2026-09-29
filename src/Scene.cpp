#include <channel/System.h>
#include <channel/Scene.h>
#include <channel/LayoutObj.h>
#include <channel/Color.h>

#include <channel/SimpleModel.h>

#include <nw4r/ef.h>
#include <nw4r/ut.h>

#include <revolution/GX.h>
#include <revolution/SC.h>
#include <revolution/VI.h>
#include <revolution/OS.h>

// Layout allocator (nw4r::lyt)
extern "C" MEMAllocator* lbl_803311C8;
// Error screen layout archive
extern "C" u8 lbl_8019C6C0[];
// Cursor textures
extern "C" TPLPalette* gCommonTpl;
// Scratch matrix for SimpleModel::CalcMtx
extern "C" nw4r::math::MTX34 lbl_801F6B28;

// Not yet decompiled (weather)
void RotateMtxDeg(nw4r::math::MTX34* mtx, f32 x, f32 y, f32 z);
void TranslateMtx(nw4r::math::MTX34* mtx, f32 x, f32 y, f32 z);

// nw4r::ef memory manager, configured and compiled by the channel (global namespace, see ef_memorymanagerconfig.h)
class MemoryManager : public nw4r::ef::MemoryManagerBase {
public:
    MemoryManager(void* pStartAddr, u32 size, int maxEffect, int maxEmitter, int maxParticleManager,
                  int maxParticle);
    virtual ~MemoryManager();

    virtual void GarbageCollection();

    virtual nw4r::ef::Effect* AllocEffect();
    virtual void FreeEffect(void* pObject);
    virtual u32 GetNumAllocEffect() const;
    virtual u32 GetNumActiveEffect() const;
    virtual u32 GetNumFreeEffect() const;

    virtual nw4r::ef::Emitter* AllocEmitter();
    virtual void FreeEmitter(void* pObject);
    virtual u32 GetNumAllocEmitter() const;
    virtual u32 GetNumActiveEmitter() const;
    virtual u32 GetNumFreeEmitter() const;

    virtual nw4r::ef::ParticleManager* AllocParticleManager();
    virtual void FreeParticleManager(void* pObject);
    virtual u32 GetNumAllocParticleManager() const;
    virtual u32 GetNumActiveParticleManager() const;
    virtual u32 GetNumFreeParticleManager() const;

    virtual nw4r::ef::Particle* AllocParticle();
    virtual void FreeParticle(void* pObject);
    virtual u32 GetNumAllocParticle() const;
    virtual u32 GetNumActiveParticle() const;
    virtual u32 GetNumFreeParticle() const;

    virtual void* AllocHeap(u32 size);
    virtual void FreeHeap(void* pPtr);

    u8 unk4[0x4C - 0x4]; // at 0x4
};

// Cursor type -> effect style (normal, hold, open); type 5 draws a plain texture instead
static const s32 sCursorStyle[] = {0, 0, 1, 2, 2, -1};
// Starting spin counter per controller
static const s32 sSpinStart[WPAD_MAX_CONTROLLERS] = {0, 4, 2, 6};

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

static inline nw4r::ef::Effect* CreateEffect(const char* name) {
    return nw4r::ef::EffectSystem::GetInstance()->CreateEffect(name, 0, 0);
}

static inline void SetEffectTranslate(nw4r::ef::Effect* effect, const nw4r::math::VEC3& pos) {
    nw4r::ef::Emitter* emitter = effect->GetRootEmitter();
    emitter->mParameter.mTranslate.x = pos.x;
    emitter->mParameter.mTranslate.y = pos.y;
    emitter->mParameter.mTranslate.z = pos.z;
    emitter->SetMtxDirty();
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
    nw4r::ef::Resource* resource;
    u32 texSize;

    mEffectReady = FALSE;
    mEffectHeap = MEM2Alloc(0x20000, 32);
    mEffectMemory = new MemoryManager(mEffectHeap, 0x20000, 32, 64, 64, 64);
    nw4r::ef::EffectSystem::GetInstance()->SetMemoryManager(mEffectMemory, 1);

    mEffectData = NULL;
    mEffectTexData = NULL;
    resource = nw4r::ef::Resource::GetInstance();
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
        mCursorEffect[i] = NULL;
        mSpinEffect[i] = NULL;
        mShadowEffect[i] = NULL;
        mSpinCounter[i] = 0;
    }
}

void Cursor::Reset() {
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mType[i] = 0;
    }
}

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

void Cursor::Calc() {
    nw4r::math::MTX34 mtx;
    nw4r::math::VEC3 zero;
    f32 rotation[WPAD_MAX_CONTROLLERS];
    nw4r::math::VEC3 shadowPos;
    nw4r::math::VEC3 spinPos;
    nw4r::math::VEC3 cursorPos;
    nw4r::ef::EffectSystem* system = nw4r::ef::EffectSystem::GetInstance();

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        if (mCursorEffect[i] != NULL) {
            mCursorEffect[i]->RetireEmitterAll();
            mCursorEffect[i] = NULL;
        }
        if (mSpinEffect[i] != NULL) {
            mSpinEffect[i]->RetireEmitterAll();
            mSpinEffect[i] = NULL;
        }
        if (mShadowEffect[i] != NULL) {
            mShadowEffect[i]->RetireEmitterAll();
            mShadowEffect[i] = NULL;
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

            mShadowEffect[chan] = CreateEffect(sShadowEffects[sCursorStyle[mType[chan]]]);
            if (mShadowEffect[chan] != NULL) {
                f32 pointerY = gPointerY[chan][0];
                f32 pointerX = gPointerX[chan][0];
                shadowPos.y = ((456.0f - pointerY) - 3.0f) - offsetY;
                shadowPos.x = offsetX + (3.0f + pointerX);
                shadowPos.z = 0.0f;
                SetEffectTranslate(mShadowEffect[chan], shadowPos);
            }

            if (mType[chan] == 4) {
                mSpinEffect[chan] = CreateEffect(sCursorEffects[2][chan]);
                if (mSpinEffect[chan] != NULL) {
                    spinPos.x = gPointerX[chan][0] - offsetX;
                    spinPos.y = offsetY + (456.0f - gPointerY[chan][0]);
                    spinPos.z = 0.0f;
                    SetEffectTranslate(mSpinEffect[chan], spinPos);
                }
            }

            mCursorEffect[chan] = CreateEffect(sCursorEffects[sCursorStyle[mType[chan]]][chan]);
            if (mCursorEffect[chan] != NULL) {
                cursorPos.x = offsetX + gPointerX[chan][0];
                cursorPos.y = (456.0f - gPointerY[chan][0]) - offsetY;
                cursorPos.z = 0.0f;
                SetEffectTranslate(mCursorEffect[chan], cursorPos);
            }
        }
        mType[chan] = 0;
    }

    zero.x = 0.0f;
    zero.y = 0.0f;
    zero.z = 0.0f;
    PSMTXIdentity(mtx);
    system->SetProcessCamera(-100.0f, 100.0f, zero, mtx);
    system->Calc(0, false);

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        if (mCursorEffect[i] != NULL) {
            UpdateParticles(mCursorEffect[i], rotation[i], 1.0f);
        }
        if (mSpinEffect[i] != NULL) {
            UpdateParticles(mSpinEffect[i], rotation[i], 0.85f);
        }
        if (mShadowEffect[i] != NULL) {
            UpdateParticles(mShadowEffect[i], rotation[i], 1.0f);
        }
    }
}

void Cursor::Draw() {
    nw4r::ef::DrawInfo info;
    nw4r::math::MTX34 view;
    Mtx44 proj;
    f32 width = GetScreenWidth();

    // Effects are drawn with Y pointing up
    C_MTXOrtho(proj, 456.0f, 0.0f, 0.0f, width, -100.0f, 100.0f);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
    SetDefaultGXState();

    PSMTXIdentity(view);
    info.SetViewMtx(view);
    nw4r::ef::EffectSystem::GetInstance()->Draw(info, 0);
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

    SetVec(&shadowPos, GetPointerX(chan) + 3.0f, GetPointerY(chan) + 3.0f, 0.0f);
    MakeTransformMtx(&scale, &rotation, &shadowPos, mtx);
    GXLoadPosMtxImm(mtx, GX_PNMTX1);
    GXColor shadowColor = {0, 0, 0, 255};
    GXSetTevColor(GX_TEVREG0, shadowColor);
    DrawTextureAt(gCommonTpl, 2, 1.0f, 1.0f, &offset);

    SetVec(&pos, GetPointerX(chan), GetPointerY(chan), 0.0f);
    MakeTransformMtx(&scale, &rotation, &pos, mtx);
    GXLoadPosMtxImm(mtx, GX_PNMTX1);
    GXColor color = {255, 255, 255, 255};
    GXSetTevColor(GX_TEVREG0, color);
    DrawTextureAt(gCommonTpl, 1, 1.0f, 1.0f, &offset);
}

// Sets rotation on all live particles of an effect and scales their colors
void Cursor::UpdateParticles(nw4r::ef::Effect* effect, f32 rotation, f32 brightness) {
    for (u16 i = 0; i < effect->GetNumEmitter(); i++) {
        nw4r::ef::Emitter* emitter = effect->GetEmitter(i);

        for (u16 j = 0; j < emitter->GetNumParticleManager(); j++) {
            nw4r::ef::ParticleManager* manager = emitter->GetParticleManager(j);
            nw4r::ut::List* list = &manager->GetParticleList()->mActiveList;
            nw4r::ef::Particle* particle = NULL;

            while ((particle = static_cast<nw4r::ef::Particle*>(nw4r::ut::List_GetNext(list, particle))) != NULL) {
                if (particle->GetLifeStatus() != nw4r::ef::ReferencedObject::NW4R_EF_LS_ACTIVE &&
                    particle->GetLifeStatus() != nw4r::ef::ReferencedObject::NW4R_EF_LS_WAIT) {
                    continue;
                }

                particle->mParameter.mRotate.z = rotation;

                GXColor* color = particle->mParameter.mColor[0];
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

SimpleModel::SimpleModel(void* resData) : mTrans(0.0f, 0.0f, 0.0f), mRot(0.0f, 0.0f, 0.0f), mScale(21.0f, 21.0f, 21.0f) {
    nw4r::g3d::ResFile file(resData);
    u32 size;

    file.Init();
    file.Bind();
    mResMdl = file.GetResMdl(0);
    mScnMdl = nw4r::g3d::ScnMdlSimple::Construct(&gMEM2Allocator32, &size, mResMdl, 1);
}

SimpleModel::~SimpleModel() {
    mScnMdl->Destroy();
}

// Empty in this build
void SimpleModel::Calc() {}

void SimpleModel::UpdateMtx() {
    mMtx = CalcMtx(mRot);
    mScnMdl->SetMtx(nw4r::g3d::ScnObj::MTX_LOCAL, &mMtx);
}

nw4r::math::MTX34 SimpleModel::CalcMtx(const nw4r::math::VEC3& rot) {
    nw4r::math::MTX34RotXYZFIdx(&lbl_801F6B28, 0.0f, 0.7111111f * rot.y, 0.0f);
    f32 z = rot.z;
    f32 x = rot.x;
    RotateMtxDeg(&lbl_801F6B28, x, 0.0f, z);
    TranslateMtx(&lbl_801F6B28, mTrans.x, mTrans.y, mTrans.z);
    return lbl_801F6B28;
}

// Empty in this build
void SimpleModel::Draw() {}
