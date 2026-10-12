#include <nw4r/snd.h>
#include <nw4r/ut.h>

#include <revolution/WPAD.h>

namespace nw4r {
namespace snd {
namespace detail {

void WsdTrack::Init(WsdPlayer* pPlayer) {
    mPlayer = pPlayer;

    mWsdData = NULL;
    mWsdIndex = -1;

    mPriority = DEFAULT_PRIORITY;
    mBendRange = DEFAULT_BENDRANGE;

    mWaveSoundInfo.pitch = 1.0f;
    mWaveSoundInfo.pan = 64;
    mWaveSoundInfo.surroundPan = 0;
    mWaveSoundInfo.fxSendA = 0;
    mWaveSoundInfo.fxSendB = 0;
    mWaveSoundInfo.fxSendC = 0;
    mWaveSoundInfo.mainSend = 127;

    mLfoParam.Init();

    mNoteCount = 0;
    mChannelList = NULL;
}

void WsdTrack::SetData(const void* pWsdData, int index) {
    mWsdData = pWsdData;
    mWsdIndex = index;
}

void WsdTrack::Close() {
    ReleaseAllChannel(-1);
    FreeAllChannel();
}

int WsdTrack::ParseNextTick(const WsdCallback* pCallback, u32 callbackArg,
                            bool doNoteOn) {
    for (Channel* pIt = mChannelList; pIt != NULL;
         pIt = pIt->GetNextTrackChannel()) {

        if (pIt->GetLength() > 0) {
            pIt->SetLength(pIt->GetLength() - 1);

            if (!pIt->IsAutoUpdateSweep()) {
                pIt->UpdateSweep(1);
            }

            if (pIt->GetLength() == 0) {
                pIt->Release();
            }
        }
    }

    if (mNoteCount != 0 && mChannelList == NULL) {
        return -1;
    }

    return Parse(pCallback, callbackArg, doNoteOn);
}

void WsdTrack::ReleaseAllChannel(int release) {
    UpdateChannel();

    ut::AutoInterruptLock lock;

    VoiceManager& rManager = VoiceManager::GetInstance();
    rManager.DisableUpdateVoicePriority();

    for (Channel* pIt = mChannelList; pIt != NULL;
         pIt = pIt->GetNextTrackChannel()) {

        if (pIt->IsActive()) {
            if (release >= 0) {
                pIt->SetRelease(static_cast<u8>(release));
            }

            pIt->Release();
        }
    }

    VoiceManager::GetInstance().EnableUpdateVoicePriority();
}

void WsdTrack::PauseAllChannel(bool flag) {
    for (Channel* pIt = mChannelList; pIt != NULL;
         pIt = pIt->GetNextTrackChannel()) {

        if (pIt->IsActive() && flag != pIt->IsPause()) {
            pIt->Pause(flag);
        }
    }
}

void WsdTrack::UpdateChannel() {
    f32 volume = 1.0f;
    volume *= mPlayer->GetVolume();

    f32 pitchRatio = 1.0f;
    pitchRatio *= mPlayer->GetPitch();
    pitchRatio *= mWaveSoundInfo.pitch;

    f32 pan = 0.0f;
    if (mWaveSoundInfo.pan <= 1) {
        pan += (mWaveSoundInfo.pan - 63) / 63.0f;
    } else {
        pan += (mWaveSoundInfo.pan - 64) / 63.0f;
    }
    pan *= mPlayer->GetPanRange();
    pan += mPlayer->GetPan();

    f32 surroundPan = 0.0f;
    if (mWaveSoundInfo.surroundPan <= 1) {
        surroundPan += (mWaveSoundInfo.surroundPan + 1) / 63.0f;
    } else {
        surroundPan += mWaveSoundInfo.surroundPan / 63.0f;
    }
    surroundPan += mWaveSoundInfo.surroundPan / 64.0f;
    surroundPan += mPlayer->GetSurroundPan();

    f32 ambientPan = 0.0f;
    ambientPan += mPlayer->GetAmbientPan();

    f32 ambientSurroundPan = 0.0f;
    ambientSurroundPan += mPlayer->GetAmbientSurroundPan();

    f32 lpfFreq = 0.0f;
    lpfFreq += mPlayer->GetLpfFreq();

    // The older library starts from 1.0 here (it cancels out below)
    f32 mainSend = 1.0f;
    mainSend += (mWaveSoundInfo.mainSend / 127.0f) - 1.0f;
    mainSend += mPlayer->GetMainSend();

    u8 infoSend[AUX_BUS_NUM];
    infoSend[AUX_A] = mWaveSoundInfo.fxSendA;
    infoSend[AUX_B] = mWaveSoundInfo.fxSendB;
    infoSend[AUX_C] = mWaveSoundInfo.fxSendC;

    f32 fxSend[AUX_BUS_NUM];
    for (int i = 0; i < AUX_BUS_NUM; i++) {
        fxSend[i] = 0.0f;
        fxSend[i] += infoSend[i] / 127.0f;
        fxSend[i] += mPlayer->GetFxSend(static_cast<AuxBus>(i));
    }

    f32 remoteSend[WPAD_MAX_CONTROLLERS];
    f32 remoteFxSend[WPAD_MAX_CONTROLLERS];
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        remoteSend[i] = 0.0f;
        remoteSend[i] += mPlayer->GetRemoteSend(i);

        remoteFxSend[i] = 0.0f;
        remoteFxSend[i] += mPlayer->GetRemoteFxSend(i);
    }

    ut::AutoInterruptLock lock;

    for (Channel* pIt = mChannelList; pIt != NULL;
         pIt = pIt->GetNextTrackChannel()) {

        pIt->SetUserVolume(volume);
        pIt->SetUserPitchRatio(pitchRatio);
        pIt->SetUserPan(pan);
        pIt->SetUserSurroundPan(surroundPan);
        pIt->SetUserAmbientPan(ambientPan);
        pIt->SetUserAmbientSurroundPan(ambientSurroundPan);
        pIt->SetUserLpfFreq(lpfFreq);
        pIt->SetOutputLine(mPlayer->GetOutputLine());
        pIt->SetMainOutVolume(mPlayer->GetMainOutVolume());
        pIt->SetMainSend(mainSend);

        for (int i = 0; i < AUX_BUS_NUM; i++) {
            pIt->SetFxSend(static_cast<AuxBus>(i), fxSend[i]);
        }

        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
            pIt->SetRemoteOutVolume(i, mPlayer->GetRemoteOutVolume(i));
            pIt->SetRemoteSend(i, remoteSend[i]);
            pIt->SetRemoteFxSend(i, remoteFxSend[i]);
        }

        pIt->SetLfoParam(mLfoParam);
    }
}

int WsdTrack::Parse(const WsdCallback* pCallback, u32 callbackArg,
                    bool doNoteOn) {
#pragma unused(doNoteOn)

    if (mNoteCount == 0) {
        int priority = mPriority + mPlayer->GetChannelPriority();

        WaveSoundNoteInfo noteInfo;
        WaveData waveData;

        if (!pCallback->GetWaveSoundData(&mWaveSoundInfo, &noteInfo, &waveData,
                                         mWsdData, mWsdIndex, 0,
                                         callbackArg)) {
            return -1;
        }

        Channel* pChannel = Channel::AllocChannel(
            ut::Min<int>(waveData.numChannels, CHANNEL_MAX),
            mPlayer->GetVoiceOutCount(), priority, ChannelCallbackFunc,
            reinterpret_cast<u32>(this));

        if (pChannel == NULL) {
            return -1;
        }

        pChannel->SetAttack(noteInfo.attack);
        pChannel->SetDecay(noteInfo.decay);
        pChannel->SetSustain(noteInfo.sustain);
        pChannel->SetRelease(noteInfo.release);

        pChannel->Start(waveData, -1);

        pChannel->SetNextTrackChannel(mChannelList);
        mChannelList = pChannel;

        mNoteCount++;
    } else if (mChannelList == NULL) {
        return -1;
    }

    return 0;
}

void WsdTrack::ChannelCallbackFunc(Channel* pDropChannel,
                                   Channel::ChannelCallbackStatus status,
                                   u32 callbackArg) {
    WsdTrack* p = reinterpret_cast<WsdTrack*>(callbackArg);

    if (status == Channel::CALLBACK_STATUS_FINISH) {
        Channel::FreeChannel(pDropChannel);
    }

    if (p->mChannelList == pDropChannel) {
        p->mChannelList = pDropChannel->GetNextTrackChannel();
        return;
    }

    for (Channel* pIt = p->mChannelList; pIt->GetNextTrackChannel() != NULL;
         pIt = pIt->GetNextTrackChannel()) {

        if (pIt->GetNextTrackChannel() == pDropChannel) {
            pIt->SetNextTrackChannel(pDropChannel->GetNextTrackChannel());
            return;
        }
    }
}

} // namespace detail
} // namespace snd
} // namespace nw4r
