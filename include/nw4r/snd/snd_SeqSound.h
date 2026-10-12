#ifndef NW4R_SND_SEQ_SOUND_H
#define NW4R_SND_SEQ_SOUND_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_BasicSound.h>
#include <nw4r/snd/snd_SeqPlayer.h>
#include <nw4r/snd/snd_Task.h>

#include <nw4r/ut.h>

#include <revolution/OS.h>

namespace nw4r {
namespace snd {

// Forward declarations
class SeqSoundHandle;

namespace detail {
class NoteOnCallback;
class SeqTrackAllocator;
template <typename T> class SoundInstanceManager;
} // namespace detail

namespace detail {

class SeqSound : public BasicSound {
    friend class nw4r::snd::SeqSoundHandle;

public:
    NW4R_UT_RTTI_DECL(SeqSound);

public:
    typedef void (*SeqLoadCallback)(bool success, const void* pBase,
                                    s32 offset, void* pCallbackArg);

    // Interface used to load sequence data asynchronously
    class SeqLoader {
    public:
        virtual ~SeqLoader() {} // at 0x8

        virtual int LoadAsync(SeqLoadCallback pCallback, void* pCallbackArg,
                              u32 userData) = 0; // at 0xC
        virtual void Cancel(u32 userData) = 0;    // at 0x10
    };

public:
    explicit SeqSound(SoundInstanceManager<SeqSound>* pManager);

    virtual void Shutdown(); // at 0x28

    virtual void SetPlayerPriority(int priority); // at 0x4C

    virtual bool IsAttachedTempSpecialHandle(); // at 0x50
    virtual void DetachTempSpecialHandle();     // at 0x54

    virtual void InitParam(); // at 0x58

    virtual BasicPlayer& GetBasicPlayer() {
        return mSeqPlayer;
    } // at 0x5C
    virtual const BasicPlayer& GetBasicPlayer() const {
        return mSeqPlayer;
    } // at 0x60

    SeqPlayer::SetupResult Setup(SeqTrackAllocator* pAllocator,
                                 u32 allocTrackFlags, int voices,
                                 NoteOnCallback* pCallback);

    void Prepare(const void* pBase, s32 seqOffset);
    void Prepare(SeqLoader* pLoader, u32 userData);

    void SetTempoRatio(f32 tempo);
    void SetChannelPriority(int priority);

    void SetTrackMute(u32 trackFlags, bool mute);

    bool WriteVariable(int idx, s16 value);
    static bool WriteGlobalVariable(int idx, s16 value);

private:
    static void NotifyLoadAsyncEndSeqData(bool success, const void* pBase,
                                          s32 offset, void* pCallbackArg);

private:
    SeqPlayer mSeqPlayer;                     // at 0xD8
    SeqSoundHandle* mTempSpecialHandle;       // at 0x1E4
    SoundInstanceManager<SeqSound>* mManager; // at 0x1E8
    bool mLoadingFlag;                        // at 0x1EC
    SeqLoader* mLoader;                       // at 0x1F0
    u32 mLoadUserData;                        // at 0x1F4
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
