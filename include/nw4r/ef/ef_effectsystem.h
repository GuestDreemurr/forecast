#ifndef NW4R_EF_EFFECT_SYSTEM_H
#define NW4R_EF_EFFECT_SYSTEM_H
#include <nw4r/types_nw4r.h>

#include <nw4r/ef/ef_creationqueue.h>
#include <nw4r/ef/ef_random.h>
#include <nw4r/math.h>

namespace nw4r {
namespace ef {

// Forward declarations
class ActivityList;
class DrawInfo;
class DrawOrderBase;
class DrawStrategyBuilder;
class EmitFormBuilder;
class MemoryManagerBase;

class EffectSystem {
private:
    MemoryManagerBase* mMemoryManager; // at 0x0

public:
    CreationQueue mCreationQueue;  // at 0x4
    u32 mMaxGroupID;               // at 0x5008
    ActivityList* mActivityList;   // at 0x500C
    Random mRandom;                // at 0x5010
    math::VEC3 mProcessCameraPos;  // at 0x5014
    math::MTX34 mProcessCameraMtx; // at 0x5020
    f32 mProcessCameraFar;         // at 0x5050
    f32 mProcessCameraNear;        // at 0x5054
    bool mXFFlushSafe;             // at 0x5058

    static bool mDisplayVersion;
    static EffectSystem instance;

public:
    static EffectSystem* GetInstance();

    EffectSystem();
    ~EffectSystem();

    void Initialize(u32 maxGroupID);
    bool Closing(Effect* pEffect);

    Effect* CreateEffect(const char* pName, u32 groupID, u16 calcRemain);
    u32 RetireEffect(Effect* pEffect);

    u32 RetireEffectAll(u32 groupID);
    u32 RetireEmitterAll(u32 groupID);
    u32 RetireParticleAll(u32 groupID);

    void Calc(u32 groupID, bool onlyBillboard);
    void Draw(const DrawInfo& rInfo, u32 groupID);

    void SetProcessCamera(f32 near, f32 far, const math::VEC3& rPos,
                          const math::MTX34& rMtx);

    MemoryManagerBase* GetMemoryManager() const {
        return mMemoryManager;
    }
    void SetMemoryManager(MemoryManagerBase* pManager, u32 maxGroupID) {
        mMemoryManager = pManager;

        if (mMemoryManager != NULL) {
            Initialize(maxGroupID);
        }
    }

    void SetXFFlushSafe(bool safe) {
        mXFFlushSafe = safe;
    }
};

} // namespace ef
} // namespace nw4r

#endif
