#include <nw4r/snd.h>
#include <nw4r/ut.h>

#include <revolution/AX.h>

namespace nw4r {
namespace snd {
namespace detail {

VoiceManager& VoiceManager::GetInstance() {
    static VoiceManager instance;
    return instance;
}

VoiceManager::VoiceManager()
    : mInitialized(false), mUpdatePriorityFlag(true) {}

u32 VoiceManager::GetRequiredMemSize() {
    return WORK_SIZE_MAX;
}

void VoiceManager::Setup(void* pBuffer, u32 size) {
    if (mInitialized) {
        return;
    }

    u32 voices = size / sizeof(Voice);
    u8* pPtr = static_cast<u8*>(pBuffer);

    for (u32 i = 0; i < voices; i++) {
        Voice* pVoice = new (pPtr) Voice();
        mFreeVoiceList.PushBack(pVoice);
        pPtr += sizeof(Voice);
    }

    mInitialized = true;
}

Voice* VoiceManager::AllocVoice(int channels, int voices, int priority,
                                Voice::VoiceCallback pCallback,
                                void* pCallbackArg) {
    ut::AutoInterruptLock lock;

    if (mFreeVoiceList.IsEmpty() && DropLowestPriorityVoice(priority) == 0) {
        return NULL;
    }

    Voice& rVoice = mFreeVoiceList.GetFront();

    if (!rVoice.Acquire(channels, voices, priority, pCallback, pCallbackArg)) {
        return NULL;
    }

    rVoice.mPriority = priority & Voice::PRIORITY_MAX;
    AppendVoiceList(&rVoice);

    if (mUpdatePriorityFlag) {
        UpdateEachVoicePriority(mPrioVoiceList.GetBeginIter(),
                                mPrioVoiceList.GetEndIter());
    }

    DisposeCallbackManager::GetInstance().RegisterDisposeCallback(&rVoice);
    return &rVoice;
}

void VoiceManager::FreeVoice(Voice* pVoice) {
    ut::AutoInterruptLock lock;

    DisposeCallbackManager::GetInstance().UnregisterDisposeCallback(pVoice);
    RemoveVoiceList(pVoice);
}

void VoiceManager::UpdateAllVoices() {
    ut::AutoInterruptLock lock;

    NW4R_UT_LINKLIST_FOREACH_SAFE (it, mPrioVoiceList, { it->Update(); })
}

void VoiceManager::DisableUpdateVoicePriority() {
    ut::AutoInterruptLock lock;
    mUpdatePriorityFlag = false;
}

void VoiceManager::EnableUpdateVoicePriority() {
    ut::AutoInterruptLock lock;

    mUpdatePriorityFlag = true;
    UpdateEachVoicePriority(mPrioVoiceList.GetBeginIter(),
                            mPrioVoiceList.GetEndIter());
}

void VoiceManager::AppendVoiceList(Voice* pVoice) {
    ut::AutoInterruptLock lock;

    mFreeVoiceList.Erase(pVoice);

    VoiceList::Iterator it = mPrioVoiceList.GetBeginIter();
    for (; it != mPrioVoiceList.GetEndIter(); ++it) {
        if (it->GetPriority() > pVoice->GetPriority()) {
            break;
        }
    }

    mPrioVoiceList.Insert(it, pVoice);
}

void VoiceManager::RemoveVoiceList(Voice* pVoice) {
    ut::AutoInterruptLock lock;

    mPrioVoiceList.Erase(pVoice);
    mFreeVoiceList.PushBack(pVoice);
}

void VoiceManager::ChangeVoicePriority(Voice* pVoice) {
    ut::AutoInterruptLock lock;

    RemoveVoiceList(pVoice);
    AppendVoiceList(pVoice);

    if (mUpdatePriorityFlag) {
        UpdateEachVoicePriority(mPrioVoiceList.GetBeginIter(),
                                mPrioVoiceList.GetEndIter());
    }
}

void VoiceManager::UpdateEachVoicePriority(const VoiceList::Iterator& rBegin,
                                           const VoiceList::Iterator& rEnd) {
    for (VoiceList::Iterator it = rBegin; it != rEnd; ++it) {
        if (it->GetPriority() != Voice::PRIORITY_MAX) {
            it->UpdateVoicesPriority();
        }
    }
}

void VoiceManager::UpdateAllVoicesSync(u32 syncFlag) {
    ut::AutoInterruptLock lock;

    NW4R_UT_LINKLIST_FOREACH_SAFE (it, mPrioVoiceList, {
        if (it->mIsActive) {
            it->mSyncFlag |= syncFlag;
        }
    })
}

int VoiceManager::DropLowestPriorityVoice(int priority) {
    int dropped = 0;

    if (mFreeVoiceList.IsEmpty()) {
        Voice& rVoice = mPrioVoiceList.GetFront();

        if (rVoice.GetPriority() > priority) {
            return 0;
        }

        dropped = rVoice.GetAxVoiceCount();

        rVoice.Stop();
        rVoice.Free();

        if (rVoice.mCallback != NULL) {
            rVoice.mCallback(&rVoice, Voice::CALLBACK_STATUS_DROP_VOICE,
                             rVoice.mCallbackArg);
        }
    }

    return dropped;
}

} // namespace detail
} // namespace snd
} // namespace nw4r
