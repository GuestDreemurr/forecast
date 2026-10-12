#include <nw4r/snd.h>
#include <nw4r/ut.h>

namespace nw4r {
namespace snd {
namespace detail {

namespace {

SeqPlayerList sPlayerList;

} // namespace

volatile s16 SeqPlayer::mGlobalVariable[GLOBAL_VARIABLE_NUM];

SeqPlayer::SeqPlayer() : mActiveFlag(false) {}

SeqPlayer::~SeqPlayer() {
    if (mActiveFlag) {
        SeqPlayer::Stop();
    }
}

void SeqPlayer::InitParam(int voices, NoteOnCallback* pCallback) {
    BasicPlayer::InitParam();

    mPreparedFlag = false;
    mStartedFlag = false;
    mPauseFlag = false;
    mSkipFlag = false;

    mTempoRatio = 1.0f;
    mTempoCounter = TEMPO_BASE;
    mPanRange = 1.0f;
    mTickCounter = 0;
    mVoiceOutCount = voices;

    mParserParam.tempo = DEFAULT_TEMPO;
    mParserParam.volume = 127;
    mParserParam.priority = DEFAULT_PRIORITY;
    mParserParam.callback = pCallback;

    for (int i = 0; i < LOCAL_VARIABLE_NUM; i++) {
        mLocalVariable[i] = DEFAULT_VARIABLE_VALUE;
    }

    for (int i = 0; i < TRACK_NUM; i++) {
        mTracks[i] = NULL;
    }
}

SeqPlayer::SetupResult SeqPlayer::Setup(SeqTrackAllocator* pAllocator,
                                        u32 allocTrackFlags, int voices,
                                        NoteOnCallback* pCallback) {
    ut::AutoInterruptLock lock;

    if (mActiveFlag) {
        SeqPlayer::Stop();
    }

    InitParam(voices, pCallback);

    u32 trackFlags = allocTrackFlags;
    bool success = true;
    {
        for (int i = 0; trackFlags != 0; trackFlags >>= 1, i++) {
            if (trackFlags & 1) {
                SeqTrack* pTrack = pAllocator->AllocTrack(this);
                if (pTrack == NULL) {
                    success = false;
                    break;
                }

                SetPlayerTrack(i, pTrack);
            }
        }
    }

    if (!success) {
        u32 trackFlags = allocTrackFlags;

        for (int i = 0; trackFlags != 0; trackFlags >>= 1, i++) {
            if (trackFlags & 1) {
                SeqTrack* pTrack = GetPlayerTrack(i);

                if (pTrack != NULL) {
                    pAllocator->FreeTrack(pTrack);
                }
            }
        }

        return SETUP_ERR_CANNOT_ALLOCATE_TRACK;
    }

    DisposeCallbackManager::GetInstance().RegisterDisposeCallback(this);

    mSeqTrackAllocator = pAllocator;
    mActiveFlag = true;

    return SETUP_SUCCESS;
}

void SeqPlayer::SetSeqData(const void* pBase, s32 offset) {
    SeqTrack* pTrack = GetPlayerTrack(0);

    if (pBase != NULL) {
        pTrack->SetSeqData(pBase, offset);
        pTrack->Open();
    }

    mPreparedFlag = true;
}

bool SeqPlayer::Start() {
    ut::AutoInterruptLock lock;

    if (!mPreparedFlag) {
        return false;
    }

    sPlayerList.PushBack(this);
    mStartedFlag = true;

    return true;
}

void SeqPlayer::Stop() {
    ut::AutoInterruptLock lock;
    FinishPlayer();
}

void SeqPlayer::Pause(bool flag) {
    ut::AutoInterruptLock lock;

    mPauseFlag = static_cast<u8>(flag) != 0;

    for (int i = 0; i < TRACK_NUM; i++) {
        SeqTrack* pTrack = GetPlayerTrack(i);

        if (pTrack != NULL) {
            pTrack->PauseAllChannel(flag);
        }
    }
}

void SeqPlayer::SetTempoRatio(f32 tempo) {
    mTempoRatio = tempo;
}

void SeqPlayer::SetChannelPriority(int priority) {
    ut::AutoInterruptLock lock;
    mParserParam.priority = priority;
}

void SeqPlayer::SetLocalVariable(int idx, s16 value) {
    mLocalVariable[idx] = value;
}

void SeqPlayer::SetGlobalVariable(int idx, s16 value) {
    mGlobalVariable[idx] = value;
}

void SeqPlayer::SetTrackMute(u32 trackFlags, SeqMute mute) {
    SetTrackParam<SeqMute>(trackFlags, &SeqTrack::SetMute, mute);
}

void SeqPlayer::InvalidateData(const void* pStart, const void* pEnd) {
    ut::AutoInterruptLock lock;

    if (mActiveFlag) {
        for (int i = 0; i < TRACK_NUM; i++) {
            SeqTrack* pTrack = GetPlayerTrack(i);
            if (pTrack == NULL) {
                continue;
            }

            const u8* pBase = pTrack->GetParserTrackParam().baseAddr;

            if (pStart <= pBase && pBase <= pEnd) {
                SeqPlayer::Stop();
                break;
            }
        }
    }
}

SeqTrack* SeqPlayer::GetPlayerTrack(int idx) {
    if (idx > TRACK_NUM - 1) {
        return NULL;
    }

    return mTracks[idx];
}

void SeqPlayer::CloseTrack(int idx) {
    SeqTrack* pTrack = GetPlayerTrack(idx);
    if (pTrack == NULL) {
        return;
    }

    pTrack->Close();

    mSeqTrackAllocator->FreeTrack(mTracks[idx]);
    mTracks[idx] = NULL;
}

void SeqPlayer::SetPlayerTrack(int idx, SeqTrack* pTrack) {
    if (idx > TRACK_NUM - 1) {
        return;
    }

    mTracks[idx] = pTrack;
    pTrack->SetPlayerTrackNo(idx);
}

void SeqPlayer::FinishPlayer() {
    if (mStartedFlag) {
        sPlayerList.Erase(this);
        mStartedFlag = false;
    }

    if (mActiveFlag) {
        DisposeCallbackManager::GetInstance().UnregisterDisposeCallback(this);
        mActiveFlag = false;
    }

    for (int i = 0; i < TRACK_NUM; i++) {
        CloseTrack(i);
    }
}

void SeqPlayer::UpdateChannelParam() {
    for (int i = 0; i < TRACK_NUM; i++) {
        SeqTrack* pTrack = GetPlayerTrack(i);

        if (pTrack != NULL) {
            pTrack->UpdateChannelParam();
        }
    }
}

int SeqPlayer::ParseNextTick(bool doNoteOn) {
    bool active = false;

    for (int i = 0; i < TRACK_NUM; i++) {
        SeqTrack* pTrack = GetPlayerTrack(i);
        if (pTrack == NULL) {
            continue;
        }

        pTrack->UpdateChannelLength();

        if (pTrack->ParseNextTick(doNoteOn) < 0) {
            CloseTrack(i);
        }

        if (pTrack->IsOpened()) {
            active = true;
        }
    }

    if (!active) {
        return 1;
    }

    return 0;
}

volatile s16* SeqPlayer::GetVariablePtr(int idx) {
    if (idx < LOCAL_VARIABLE_NUM) {
        return &mLocalVariable[idx];
    }

    if (idx < VARIABLE_NUM) {
        return &mGlobalVariable[idx - LOCAL_VARIABLE_NUM];
    }

    return NULL;
}

void SeqPlayer::Update() {
    if (!mActiveFlag) {
        return;
    }

    if (!mStartedFlag) {
        return;
    }

    if (!mPauseFlag && !mSkipFlag) {
        int ticks = 0;

        while (mTempoCounter >= TEMPO_BASE) {
            mTempoCounter -= TEMPO_BASE;
            ticks++;
        }

        f32 tempo = mParserParam.tempo;
        tempo *= mTempoRatio;
        mTempoCounter += static_cast<int>(tempo);

        for (; ticks > 0; ticks--) {
            if (ParseNextTick(true) != 0) {
                SeqPlayer::Stop();
                break;
            }

            mTickCounter++;
        }
    }

    UpdateChannelParam();
}

void SeqPlayer::UpdateAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->Update(); })
}

void SeqPlayer::StopAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->Stop(); })
}

Channel* SeqPlayer::NoteOn(int bankNo, const NoteOnInfo& rInfo) {
    return mParserParam.callback->NoteOn(this, bankNo, rInfo);
}

void SeqPlayer::ChannelCallback(Channel* /* pChannel */) {}

} // namespace detail
} // namespace snd
} // namespace nw4r
