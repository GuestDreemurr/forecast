#ifndef NW4R_EF_EMITTER_H
#define NW4R_EF_EMITTER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/ef/ef_activitylist.h>
#include <nw4r/ef/ef_animcurve.h>
#include <nw4r/ef/ef_random.h>
#include <nw4r/ef/ef_referencedobject.h>
#include <nw4r/ef/ef_res_emitter.h>
#include <nw4r/ef/ef_types.h>
#include <nw4r/math.h>
#include <nw4r/ut.h>

namespace nw4r {
namespace ef {

// Forward declarations
class Effect;
class Emitter;
class EmitterForm;
class Particle;
class ParticleManager;
struct EmitterResource;

struct EmitterInheritSetting {
    enum Flag {
        FLAG_FOLLOW_EMIT = (1 << 0),
        FLAG_INHERIT_ROT = (1 << 1),
    };

    s16 speed; // at 0x0
    u8 scale;  // at 0x2
    u8 alpha;  // at 0x3
    u8 color;  // at 0x4
    u8 weight; // at 0x5
    u8 type;   // at 0x6
    u8 flag;   // at 0x7
};

class EmitterParameter {
public:
    enum InheritFlag {
        INHERIT_FLAG_SCALE = (1 << 0),
        INHERIT_FLAG_ROT = (1 << 1)
    };

public:
    u32 mComFlags;                  // at 0x0
    u32 mEmitFlags;                 // at 0x4
    f32 mEmitRatio;                 // at 0x8
    f32 mEmitRandom;                // at 0xC
    u16 mEmitInterval;              // at 0x10
    u16 mEmitEmitDiv;               // at 0x12
    f32 mEmitCount;                 // at 0x14
    u16 mEmitSpan;                  // at 0x18
    f32 mLODNear;                   // at 0x1C
    f32 mLODFar;                    // at 0x20
    f32 mLODMinEmit;                // at 0x24
    f32 mParams[NUM_PARAMS];        // at 0x28
    u8 mInherit;                    // at 0x40
    s8 mInheritTranslate;           // at 0x41
    s8 mVelInitVelocityRandom;      // at 0x42
    s8 mVelMomentumRandom;          // at 0x43
    f32 mVelPowerRadiationDir;      // at 0x44
    f32 mVelPowerYAxis;             // at 0x48
    f32 mVelPowerRandomDir;         // at 0x4C
    f32 mVelPowerNormalDir;         // at 0x50
    f32 mVelDiffusionEmitterNormal; // at 0x54
    f32 mVelPowerSpecDir;           // at 0x58
    f32 mVelDiffusionSpecDir;       // at 0x5C
    math::VEC3 mVelSpecDir;         // at 0x60
    math::VEC3 mTranslate;          // at 0x6C
    math::VEC3 mScale;              // at 0x78
    math::VEC3 mRotate;             // at 0x84
};

class Emitter : public ReferencedObject {
    friend class Effect;

public:
    EmitterParameter mParameter;           // at 0x20
    EvaluateStatus mEvalStatus;            // at 0xB0
    EmitterResource* mResource;            // at 0xB4
    Effect* mManagerEF;                    // at 0xB8
    ActivityList mActivityList;            // at 0xBC
    bool mIsFirstEmission;                 // at 0xD8
    u16 mWaitTime;                         // at 0xDA
    u16 mEmitIntervalWait;                 // at 0xDC
    u32 mTick;                             // at 0xE0
    u16 mCalcRemain;                       // at 0xE4
    u16 mRandSeed;                         // at 0xE6
    Random mRandom;                        // at 0xE8
    EmitterForm* mForm;                    // at 0xEC
    Emitter* mParent;                      // at 0xF0
    Particle* mpReferenceParticle;         // at 0xF4
    EmitterInheritSetting mInheritSetting; // at 0xF8

protected:
    bool mMtxDirty; // at 0x100

public:
    math::MTX34 mMtx; // at 0x104

public:
    Emitter();
    ~Emitter();

    virtual void SendClosing(); // at 0x8
    virtual void DestroyFunc(); // at 0xC

    bool Initialize(Effect* pParent, EmitterResource* pResource, u8 drawWeight);

    Emitter* CreateEmitter(EmitterResource* pResource,
                           EmitterInheritSetting* pSetting, Particle* pParticle,
                           u16 calcRemain);

    void CalcEmitter();
    void CalcParticle();
    void CalcEmission();
    void CalcBillboard();

    bool Closing(ParticleManager* pManager);

    u32 RetireParticleAll();
    u32 RetireParticleManager(ParticleManager* pManager);
    u32 RetireParticleManagerAll();

    void UpdateDatas(EmitterResource* pResource);
    bool InitializeDatas(EmitterResource* pResource, Effect* pEffect);

    void CreateEmitterTmp(EmitterResource* pResource,
                          EmitterInheritSetting* pSetting, Particle* pParticle,
                          u16 calcRemain);

    ParticleManager* FindParticleManager(EmitterResource* pResource,
                                         bool inheritS, bool inheritR,
                                         s8 inheritT, u8 weight);

    void Emission(ParticleManager* pManager, const math::MTX34* pSpace);

    math::MTX34* CalcGlobalMtx(math::MTX34* pResult);
    void SetMtxDirty();

    u16 GetNumParticleManager() const;
    ParticleManager* GetParticleManager(u16 idx);

    u32 ForeachEmitter(ForEachFunc pFunc, ForEachParam param,
                       bool ignoreLifeStatus);
    u32 ForeachParticleManager(ForEachFunc pFunc, ForEachParam param,
                               bool ignoreLifeStatus, bool propogate);

    static math::MTX34* RestructMatrix(math::MTX34* pResult, math::MTX34* pOrig,
                                       bool inheritS, bool inheritR,
                                       s8 inheritT);

    bool GetFlagDisableCalc() const {
        return mParameter.mComFlags & EmitterDesc::CMN_FLAG_DISABLE_CALC;
    }

    void SetTranslate(const math::VEC3& rTrans) {
        mParameter.mTranslate = rTrans;
    }
    void SetScale(const math::VEC3& rScale) {
        mParameter.mScale = rScale;
    }
};

} // namespace ef
} // namespace nw4r

#endif
