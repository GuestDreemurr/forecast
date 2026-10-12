#include <nw4r/snd.h>
#include <nw4r/ut.h>

namespace nw4r {
namespace snd {
namespace detail {
namespace {

WsdPlayerList sPlayerList;

} // namespace

/******************************************************************************
 *
 * Inline implementation
 *
 ******************************************************************************/
void WsdPlayer::InitParam(int voices, const WsdCallback* pCallback,
                          u32 callbackArg) {
    BasicPlayer::InitParam();

    mPreparedFlag = false;
    mStartedFlag = false;
    mPauseFlag = false;
    mSkipFlag = false;

    mPanRange = 1.0f;
    mTickCounter = 0;
    mVoiceOutCount = voices;
    mPriority = DEFAULT_PRIORITY;

    mCallback = pCallback;
    mCallbackData = callbackArg;

    mTrack.Init(this);
}

void WsdPlayer::FinishPlayer() {
    ut::AutoInterruptLock lock;

    if (mStartedFlag) {
        sPlayerList.Erase(this);
        mStartedFlag = false;
    }

    if (mActiveFlag) {
        DisposeCallbackManager::GetInstance().UnregisterDisposeCallback(this);
        mActiveFlag = false;
    }

    mTrack.Close();
}

bool WsdPlayer::ParseNextTick(bool doNoteOn) {
    bool active = false;

    if (mTrack.ParseNextTick(mCallback, mCallbackData, doNoteOn) >= 0) {
        active = true;
    } else {
        mTrack.Close();
    }

    return active;
}

void WsdPlayer::Update() {
    if (!mActiveFlag) {
        return;
    }

    if (!mStartedFlag) {
        return;
    }

    if (!mPauseFlag && !mSkipFlag) {
        if (!ParseNextTick(true)) {
            FinishPlayer();
            return;
        }
    }

    mTrack.UpdateChannel();
}

/******************************************************************************
 *
 * WsdPlayer
 *
 ******************************************************************************/
WsdPlayer::WsdPlayer() : mActiveFlag(false) {}

bool WsdPlayer::Prepare(const void* pWsdData, int index, int voices,
                        const WsdCallback* pCallback, u32 callbackArg) {
    ut::AutoInterruptLock lock;

    if (mActiveFlag) {
        FinishPlayer();
    }

    InitParam(voices, pCallback, callbackArg);
    mTrack.SetData(pWsdData, index);

    DisposeCallbackManager::GetInstance().RegisterDisposeCallback(this);

    mActiveFlag = true;
    mPreparedFlag = true;

    return true;
}

bool WsdPlayer::Start() {
    ut::AutoInterruptLock lock;

    if (!mPreparedFlag) {
        return false;
    }

    sPlayerList.PushBack(this);
    mStartedFlag = true;

    return true;
}

void WsdPlayer::Stop() {
    FinishPlayer();
}

void WsdPlayer::Pause(bool flag) {
    ut::AutoInterruptLock lock;

    mPauseFlag = static_cast<u8>(flag) != 0;
    mTrack.PauseAllChannel(flag);
}

void WsdPlayer::SetChannelPriority(int priority) {
    ut::AutoInterruptLock lock;
    mPriority = priority;
}

void WsdPlayer::InvalidateData(const void* pStart, const void* pEnd) {
    ut::AutoInterruptLock lock;

    if (mActiveFlag) {
        const void* pWsdData = mTrack.GetWsdDataAddress();

        if (pStart <= pWsdData && pWsdData <= pEnd) {
            FinishPlayer();
        }
    }
}

void WsdPlayer::UpdateAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->Update(); })
}

void WsdPlayer::StopAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->Stop(); })
}

} // namespace detail
} // namespace snd
} // namespace nw4r
