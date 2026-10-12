#ifndef NW4R_SND_STRM_SOUND_H
#define NW4R_SND_STRM_SOUND_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_BasicSound.h>
#include <nw4r/snd/snd_StrmPlayer.h>

#include <nw4r/ut.h>

namespace nw4r {
namespace snd {

// Forward declarations
class StrmSoundHandle;

namespace detail {
template <typename T> class SoundInstanceManager;
class StrmBufferPool;
} // namespace detail

namespace detail {

class StrmSound : public BasicSound {
    friend class nw4r::snd::StrmSoundHandle;

public:
    NW4R_UT_RTTI_DECL(StrmSound);

public:
    explicit StrmSound(SoundInstanceManager<StrmSound>* pManager);

    virtual void Shutdown(); // at 0x28
    virtual void SetPlayerPriority(int priority); // at 0x4C
    virtual bool IsAttachedTempSpecialHandle();   // at 0x5C
    virtual void DetachTempSpecialHandle();       // at 0x60

    virtual BasicPlayer& GetBasicPlayer() {
        return mStrmPlayer;
    } // at 0x68
    virtual const BasicPlayer& GetBasicPlayer() const {
        return mStrmPlayer;
    } // at 0x6C

    bool Prepare(StrmBufferPool* pPool, StrmPlayer::StartOffsetType offsetType,
                 s32 offset, int voices, StrmPlayer::StrmLoader* pLoader,
                 u32 userData);

private:
    StrmPlayer mStrmPlayer;                    // at 0xD8
    StrmSoundHandle* mTempSpecialHandle;       // at 0x5F8
    SoundInstanceManager<StrmSound>* mManager; // at 0x5FC

    // Never referenced by the linked code; the sound is 0x618 bytes
    u8 UNK_0x600[0x618 - 0x600];
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
