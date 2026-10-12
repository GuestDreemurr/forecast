#include <nw4r/snd.h>
#include <nw4r/ut.h>

namespace nw4r {
namespace snd {
namespace detail {

NW4R_UT_RTTI_DEF_DERIVED(StrmSound, BasicSound);

StrmSound::StrmSound(SoundInstanceManager<StrmSound>* pManager)
    : mManager(pManager), mTempSpecialHandle(NULL) {}

bool StrmSound::Prepare(StrmBufferPool* pPool,
                        StrmPlayer::StartOffsetType offsetType, s32 offset,
                        int voices, StrmPlayer::StrmLoader* pLoader,
                        u32 userData) {
    if (pPool == NULL) {
        return false;
    }

    InitParam();

    return mStrmPlayer.Prepare(pPool, offsetType, offset, voices, pLoader,
                               userData);
}

void StrmSound::Shutdown() {
    BasicSound::Shutdown();
    mManager->Free(this);
}

void StrmSound::SetPlayerPriority(int priority) {
    BasicSound::SetPlayerPriority(priority);
    mManager->UpdatePriority(this, CalcCurrentPlayerPriority());
}

bool StrmSound::IsAttachedTempSpecialHandle() {
    return mTempSpecialHandle != NULL;
}

void StrmSound::DetachTempSpecialHandle() {
    mTempSpecialHandle->DetachSound();
}

} // namespace detail
} // namespace snd
} // namespace nw4r
