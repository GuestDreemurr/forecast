#include <nw4r/snd.h>
#include <nw4r/ut.h>

namespace nw4r {
namespace snd {
namespace detail {

NW4R_UT_RTTI_DEF_DERIVED(SeqSound, BasicSound);

SeqSound::SeqSound(SoundInstanceManager<SeqSound>* pManager)
    : mTempSpecialHandle(NULL),
      mManager(pManager),
      mLoadingFlag(false),
      mLoader(NULL) {}

void SeqSound::InitParam() {
    BasicSound::InitParam();

    mLoader = NULL;
    mLoadUserData = 0;
}

SeqPlayer::SetupResult SeqSound::Setup(SeqTrackAllocator* pAllocator,
                                       u32 allocTrackFlags, int voices,
                                       NoteOnCallback* pCallback) {
    InitParam();
    return mSeqPlayer.Setup(pAllocator, allocTrackFlags, voices, pCallback);
}

void SeqSound::Prepare(const void* pBase, s32 seqOffset) {
    mSeqPlayer.SetSeqData(pBase, seqOffset);
}

void SeqSound::Prepare(SeqLoader* pLoader, u32 userData) {
    if (pLoader != NULL) {
        pLoader->LoadAsync(NotifyLoadAsyncEndSeqData, this, userData);

        mLoader = pLoader;
        mLoadUserData = userData;
        mLoadingFlag = true;
    }
}

void SeqSound::NotifyLoadAsyncEndSeqData(bool success, const void* pBase,
                                         s32 offset, void* pCallbackArg) {
    SeqSound* p = static_cast<SeqSound*>(pCallbackArg);

    p->mLoadingFlag = false;

    if (!success) {
        p->Stop(0);
        return;
    }

    p->mSeqPlayer.SetSeqData(pBase, offset);
}

void SeqSound::Shutdown() {
    if (mLoadingFlag && mLoader != NULL) {
        mLoader->Cancel(mLoadUserData);
    }

    BasicSound::Shutdown();
    mManager->Free(this);
}

void SeqSound::SetTempoRatio(f32 tempo) {
    mSeqPlayer.SetTempoRatio(tempo);
}

void SeqSound::SetChannelPriority(int priority) {
    mSeqPlayer.SetChannelPriority(priority);
}

void SeqSound::SetPlayerPriority(int priority) {
    BasicSound::SetPlayerPriority(priority);
    mManager->UpdatePriority(this, BasicSound::CalcCurrentPlayerPriority());
}

void SeqSound::SetTrackMute(u32 trackFlags, bool mute) {
    mSeqPlayer.SetTrackMute(trackFlags, mute ? MUTE_STOP : MUTE_OFF);
}

bool SeqSound::WriteVariable(int idx, s16 value) {
    mSeqPlayer.SetLocalVariable(idx, value);
    return true;
}

bool SeqSound::WriteGlobalVariable(int idx, s16 value) {
    SeqPlayer::SetGlobalVariable(idx, value);
    return true;
}

bool SeqSound::IsAttachedTempSpecialHandle() {
    return mTempSpecialHandle != NULL;
}

void SeqSound::DetachTempSpecialHandle() {
    mTempSpecialHandle->DetachSound();
}

} // namespace detail
} // namespace snd
} // namespace nw4r
