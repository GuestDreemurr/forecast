#include <nw4r/snd.h>
#include <nw4r/ut.h>

namespace nw4r {
namespace snd {
namespace detail {

Voice::Voice()
    : mCallback(NULL),
      mIsActive(false),
      mIsStarting(false),
      mIsStarted(false),
      mIsPause(false),
      mSyncFlag(0) {

    for (int i = 0; i < CHANNEL_MAX; i++) {
        for (int j = 0; j < VOICES_MAX; j++) {
            mAxVoice[i][j] = NULL;
        }
    }
}

Voice::~Voice() {
    for (int i = 0; i < CHANNEL_MAX; i++) {
        for (int j = 0; j < VOICES_MAX; j++) {
            AxVoice* pVoice = mAxVoice[i][j];

            if (pVoice != NULL) {
                AxVoiceManager::GetInstance().FreeAxVoice(pVoice);
            }
        }
    }
}

void Voice::InitParam(int channels, int voices, VoiceCallback pCallback,
                      void* pCallbackArg) {
    ut::AutoInterruptLock lock;

    mChannelCount = channels;
    mVoiceOutCount = voices;

    mCallback = pCallback;
    mCallbackArg = pCallbackArg;

    mSyncFlag = 0;
    mIsPause = false;
    mIsPausing = false;
    mIsStarted = false;

    mVolume = 1.0f;
    mVeInitVolume = 0.0f;
    mVeTargetVolume = 1.0f;
    mLpfFreq = 1.0f;

    mPan = 0.0f;
    mSurroundPan = 0.0f;
    mAmbientPan = 0.0f;
    mAmbientSurroundPan = 0.0f;

    mOutputLineFlag = OUTPUT_LINE_MAIN;
    mMainOutVolume = 1.0f;
    mMainSend = 1.0f;

    for (int i = 0; i < AUX_BUS_NUM; i++) {
        mFxSend[i] = 0.0f;
    }

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mRemoteOutVolume[i] = 1.0f;
        mRemoteSend[i] = 1.0f;
        mRemoteFxSend[i] = 0.0f;
    }

    mPitch = 1.0f;
}

void Voice::Update() {
    ut::AutoInterruptLock lock;

    if (!mIsActive) {
        return;
    }

    if (mIsStarted && IsPlayFinished()) {
        if (mCallback != NULL) {
            mCallback(this, CALLBACK_STATUS_FINISH_WAVE, mCallbackArg);
        }

        mIsStarted = false;
        mIsStarting = false;
    }

    if ((mSyncFlag & SYNC_AX_SRC_INITIAL) && mIsStarting && !mIsStarted) {
        CalcAxSrc(true);
        RunAllAxVoice();

        mIsStarted = true;
        mSyncFlag &= ~SYNC_AX_SRC_INITIAL;
        mSyncFlag &= ~SYNC_AX_SRC;
    }

    if (mIsStarted) {
        if ((mSyncFlag & SYNC_AX_VOICE) && mIsStarting) {
            if (mIsPause || AxManager::GetInstance().IsDiskError()) {
                StopAllAxVoice();
                mIsPausing = true;
            } else {
                RunAllAxVoice();
                mIsPausing = false;
            }

            mSyncFlag &= ~SYNC_AX_VOICE;
        }

        if (mSyncFlag & SYNC_AX_SRC) {
            CalcAxSrc(false);
            mSyncFlag &= ~SYNC_AX_SRC;
        }

        if (mSyncFlag & SYNC_AX_VE) {
            if (!CalcAxVe()) {
                mSyncFlag &= ~SYNC_AX_VE;
            }
        }

        if (mSyncFlag & SYNC_AX_MIX) {
            if (!CalcAxMix()) {
                mSyncFlag &= ~SYNC_AX_MIX;
            }
        }

        if (mSyncFlag & SYNC_AX_LPF) {
            CalcAxLpf();
            mSyncFlag &= ~SYNC_AX_LPF;
        }
    }
}

bool Voice::Acquire(int channels, int voices, int priority,
                    VoiceCallback pCallback, void* pCallbackArg) {
    channels = ut::Clamp(channels, CHANNEL_MIN, CHANNEL_MAX);
    voices = ut::Clamp(voices, VOICES_MIN, VOICES_MAX);

    ut::AutoInterruptLock lock;

    u32 axPrio;
    if (priority == PRIORITY_MAX) {
        axPrio = AX_PRIORITY_MAX;
    } else {
        axPrio = (AX_PRIORITY_MAX / 2) + 1;
    }

    int required = channels * voices;
    AxVoice* voiceTable[CHANNEL_MAX * VOICES_MAX];

    for (int i = 0; required > i; i++) {
        AxVoice* pAxVoice = AxVoiceManager::GetInstance().AcquireAxVoice(
            axPrio, AxVoiceCallbackFunc, this);

        if (pAxVoice == NULL) {
            int rest = required - i;

            const VoiceList& rVoiceList =
                VoiceManager::GetInstance().GetVoiceList();

            for (VoiceList::ConstIterator it = rVoiceList.GetBeginIter();
                 it != rVoiceList.GetEndIter(); ++it) {

                if (priority < it->GetPriority()) {
                    break;
                }

                rest -= it->GetAxVoiceCount();
                if (rest <= 0) {
                    break;
                }
            }

            if (rest > 0) {
                for (int j = 0; j < i; j++) {
                    AxVoiceManager::GetInstance().FreeAxVoice(voiceTable[j]);
                }

                return false;
            }

            u32 allocPrio = axPrio;
            if (axPrio < AX_PRIORITY_MAX) {
                allocPrio = axPrio + 1;
            }

            pAxVoice = AxVoiceManager::GetInstance().AcquireAxVoice(
                allocPrio, AxVoiceCallbackFunc, this);
        }

        if (pAxVoice == NULL) {
            for (int j = 0; j < i; j++) {
                AxVoiceManager::GetInstance().FreeAxVoice(voiceTable[j]);
            }

            return false;
        }

        voiceTable[i] = pAxVoice;
    }

    int idx = 0;
    for (int i = 0; i < channels; i++) {
        for (int j = 0; j < voices; j++) {
            voiceTable[idx]->SetPriority(axPrio);
            mAxVoice[i][j] = voiceTable[idx];
            idx++;
        }
    }

    InitParam(channels, voices, pCallback, pCallbackArg);
    mIsActive = true;
    return true;
}

void Voice::Free() {
    ut::AutoInterruptLock lock;

    if (!mIsActive) {
        return;
    }

    for (int i = 0; i < mChannelCount; i++) {
        for (int j = 0; j < mVoiceOutCount; j++) {
            AxVoice* pAxVoice = mAxVoice[i][j];

            if (pAxVoice != NULL) {
                AxVoiceManager::GetInstance().FreeAxVoice(pAxVoice);
                mAxVoice[i][j] = NULL;
            }
        }
    }

    mChannelCount = 0;
    VoiceManager::GetInstance().FreeVoice(this);
    mIsActive = false;
}

void Voice::Setup(const WaveData& rData) {
    ut::AutoInterruptLock lock;

    AxVoice::Format format = WaveFormatToAxFormat(rData.sampleFormat);
    int sampleRate = rData.sampleRate;

    for (int i = 0; i < mChannelCount; i++) {
        if (mAxVoice[i][0] == NULL) {
            continue;
        }

        void* pAddr = rData.channelParam[i].dataAddr;
        const AdpcmInfo& rInfo = rData.channelParam[i].adpcmInfo;

        for (int j = 0; j < mVoiceOutCount; j++) {
            AxVoice* pAxVoice = mAxVoice[i][j];
            if (pAxVoice == NULL) {
                continue;
            }

            pAxVoice->Setup(rData.channelParam[i].dataAddr, format, sampleRate);
            pAxVoice->SetAddr(rData.loopFlag, pAddr, rData.loopStart,
                              rData.loopEnd);

            if (format == AxVoice::FORMAT_ADPCM) {
                pAxVoice->SetAdpcm(&rInfo.param);
                pAxVoice->SetAdpcmLoop(&rInfo.loopParam);
            }

            pAxVoice->SetSrcType(AxVoice::SRC_4TAP_AUTO, mPitch);
            pAxVoice->SetVoiceType(AxVoice::VOICE_TYPE_NORMAL);
        }
    }

    for (int i = 0; i < mVoiceOutCount; i++) {
        mVoiceOutParam[i].volume = 1.0f;
        mVoiceOutParam[i].pitch = 1.0f;
        mVoiceOutParam[i].pan = 0.0f;
        mVoiceOutParam[i].surroundPan = 0.0f;
        mVoiceOutParam[i].fxSend = 0.0f;
        mVoiceOutParam[i].lpf = 0.0f;
        mVoiceOutParam[i].priority = 0;
    }

    mIsPause = false;
    mIsPausing = false;

    mIsStarting = false;
    mIsStarted = false;

    mSyncFlag |= (SYNC_AX_LPF | SYNC_AX_MIX | SYNC_AX_VE);
}

void Voice::Start() {
    ut::AutoInterruptLock lock;

    mIsStarting = true;
    mIsPause = false;

    mSyncFlag |= SYNC_AX_SRC_INITIAL;
}

void Voice::Stop() {
    ut::AutoInterruptLock lock;

    if (mIsStarted) {
        StopAllAxVoice();
        mIsStarted = false;
    }

    mIsPausing = false;
    mIsPause = false;
    mIsStarting = false;
}

void Voice::Pause(bool flag) {
    ut::AutoInterruptLock lock;

    if (mIsPause == flag) {
        return;
    }

    mIsPause = flag;
    mSyncFlag |= SYNC_AX_VOICE;
}

AxVoice::Format Voice::GetFormat() const {
    ut::AutoInterruptLock lock;

    if (IsActive()) {
        return mAxVoice[0][0]->GetFormat();
    }

    return AxVoice::FORMAT_PCM16;
}

void Voice::SetVolume(f32 volume) {
    ut::AutoInterruptLock lock;

    volume = ut::Clamp(volume, 0.0f, 1.0f);

    if (volume != mVolume) {
        mVolume = volume;
        mSyncFlag |= SYNC_AX_VE;
    }
}

void Voice::SetVeVolume(f32 target, f32 init) {
    ut::AutoInterruptLock lock;

    if (init < 0.0f) {
        target = ut::Clamp(target, 0.0f, 1.0f);

        if (target != mVeTargetVolume) {
            mVeTargetVolume = target;
            mSyncFlag |= SYNC_AX_VE;
        }
    } else {
        target = ut::Clamp(target, 0.0f, 1.0f);
        init = ut::Clamp(init, 0.0f, 1.0f);

        if (init != mVeInitVolume || target != mVeTargetVolume) {
            mVeInitVolume = init;
            mVeTargetVolume = target;
            mSyncFlag |= SYNC_AX_VE;
        }
    }
}

void Voice::SetPitch(f32 pitch) {
    ut::AutoInterruptLock lock;

    if (pitch != mPitch) {
        mPitch = pitch;
        mSyncFlag |= SYNC_AX_SRC;
    }
}

void Voice::SetPan(f32 pan) {
    ut::AutoInterruptLock lock;

    pan = ut::Clamp(pan, -1.0f, 1.0f);

    if (pan != mPan) {
        mPan = pan;
        mSyncFlag |= SYNC_AX_MIX;
    }
}

void Voice::SetSurroundPan(f32 pan) {
    ut::AutoInterruptLock lock;

    pan = ut::Clamp(pan, 0.0f, 2.0f);

    if (pan != mSurroundPan) {
        mSurroundPan = pan;
        mSyncFlag |= SYNC_AX_MIX;
    }
}

void Voice::SetAmbientPan(f32 pan) {
    ut::AutoInterruptLock lock;

    if (pan != mAmbientPan) {
        mAmbientPan = pan;
        mSyncFlag |= SYNC_AX_MIX;
    }
}

void Voice::SetAmbientSurroundPan(f32 pan) {
    ut::AutoInterruptLock lock;

    pan = ut::Clamp(pan, 0.0f, 2.0f);

    if (pan != mAmbientSurroundPan) {
        mAmbientSurroundPan = pan;
        mSyncFlag |= SYNC_AX_MIX;
    }
}

void Voice::SetLpfFreq(f32 freq) {
    ut::AutoInterruptLock lock;

    freq = ut::Clamp(freq, 0.0f, 1.0f);

    if (freq != mLpfFreq) {
        mLpfFreq = freq;
        mSyncFlag |= SYNC_AX_LPF;
    }
}

void Voice::SetOutputLine(int flag) {
    ut::AutoInterruptLock lock;

    if (flag != mOutputLineFlag) {
        mOutputLineFlag = flag;
        mSyncFlag |= SYNC_AX_MIX;
    }
}

void Voice::SetMainOutVolume(f32 volume) {
    ut::AutoInterruptLock lock;

    volume = ut::Clamp(volume, 0.0f, 1.0f);

    if (volume != mMainOutVolume) {
        mMainOutVolume = volume;
        mSyncFlag |= SYNC_AX_MIX;
    }
}

void Voice::SetMainSend(f32 send) {
    ut::AutoInterruptLock lock;

    send += 1.0f;
    send = ut::Clamp(send, 0.0f, 1.0f);

    if (send != mMainSend) {
        mMainSend = send;
        mSyncFlag |= SYNC_AX_MIX;
    }
}

void Voice::SetFxSend(AuxBus bus, f32 send) {
    ut::AutoInterruptLock lock;

    send = ut::Clamp(send, 0.0f, 1.0f);

    if (send != mFxSend[bus]) {
        mFxSend[bus] = send;
        mSyncFlag |= SYNC_AX_MIX;
    }
}

void Voice::SetRemoteOutVolume(int remote, f32 volume) {
    ut::AutoInterruptLock lock;

    volume = ut::Clamp(volume, 0.0f, 1.0f);

    if (volume != mRemoteOutVolume[remote]) {
        mRemoteOutVolume[remote] = volume;
        mSyncFlag |= SYNC_AX_MIX;
    }
}

void Voice::SetRemoteSend(int remote, f32 send) {
    ut::AutoInterruptLock lock;

    send += 1.0f;
    send = ut::Clamp(send, 0.0f, 1.0f);

    if (send != mRemoteSend[remote]) {
        mRemoteSend[remote] = send;
        mSyncFlag |= SYNC_AX_MIX;
    }
}

void Voice::SetRemoteFxSend(int remote, f32 send) {
    ut::AutoInterruptLock lock;

    send = ut::Clamp(send, 0.0f, 1.0f);

    if (send != mRemoteFxSend[remote]) {
        mRemoteFxSend[remote] = send;
        mSyncFlag |= SYNC_AX_MIX;
    }
}

void Voice::SetPriority(int priority) {
    ut::AutoInterruptLock lock;

    mPriority = priority;
    VoiceManager::GetInstance().ChangeVoicePriority(this);
}

void Voice::UpdateVoicesPriority() {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < mChannelCount; i++) {
        for (int j = 0; j < mVoiceOutCount; j++) {
            AxVoice* pAxVoice = mAxVoice[i][j];

            if (pAxVoice != NULL) {
                pAxVoice->SetPriority((AX_PRIORITY_MAX / 2) + 1);
            }
        }
    }
}

bool Voice::IsCurrentAddressCoverd(int channel, const void* pBegin,
                                   const void* pEnd) const {
    ut::AutoInterruptLock lock;

    AxVoice* pAxVoice = mAxVoice[channel][0];
    if (pAxVoice == NULL) {
        return false;
    }

    return pAxVoice->IsCurrentAddressCoverd(pBegin, pEnd);
}

void Voice::SetAdpcmLoop(int channel, const AdpcmLoopParam* pParam) {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < mVoiceOutCount; i++) {
        AxVoice* pAxVoice = mAxVoice[channel][i];

        if (pAxVoice != NULL) {
            pAxVoice->SetAdpcmLoop(pParam);
        }
    }
}

void Voice::SetBaseAddress(int channel, const void* pBase) {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < mVoiceOutCount; i++) {
        AxVoice* pAxVoice = mAxVoice[channel][i];

        if (pAxVoice != NULL) {
            pAxVoice->SetBaseAddress(pBase);
        }
    }
}

bool Voice::IsPlayFinished() const {
    ut::AutoInterruptLock lock;

    if (!IsActive()) {
        return false;
    }

    return mAxVoice[0][0]->IsPlayFinished();
}

u32 Voice::GetCurrentPlayingSample() const {
    ut::AutoInterruptLock lock;

    if (!IsActive()) {
        return 0;
    }

    return mAxVoice[0][0]->GetCurrentPlayingSample();
}

void Voice::SetLoopStart(int channel, const void* pBase, u32 samples) {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < mVoiceOutCount; i++) {
        AxVoice* pAxVoice = mAxVoice[channel][i];

        if (pAxVoice != NULL) {
            pAxVoice->SetLoopStart(pBase, samples);
        }
    }
}

void Voice::SetLoopEnd(int channel, const void* pBase, u32 samples) {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < mVoiceOutCount; i++) {
        AxVoice* pAxVoice = mAxVoice[channel][i];

        if (pAxVoice != NULL) {
            pAxVoice->SetLoopEnd(pBase, samples);
        }
    }
}

void Voice::SetLoopFlag(bool loop) {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < mChannelCount; i++) {
        for (int j = 0; j < mVoiceOutCount; j++) {
            AxVoice* pAxVoice = mAxVoice[i][j];

            if (pAxVoice != NULL) {
                pAxVoice->SetLoopFlag(loop);
            }
        }
    }
}

void Voice::StopAtPoint(int channel, const void* pBase, u32 samples) {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < mVoiceOutCount; i++) {
        AxVoice* pAxVoice = mAxVoice[channel][i];

        if (pAxVoice != NULL) {
            pAxVoice->StopAtPoint(pBase, samples);
        }
    }
}

void Voice::SetVoiceType(AxVoice::VoiceType type) {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < mChannelCount; i++) {
        for (int j = 0; j < mVoiceOutCount; j++) {
            AxVoice* pAxVoice = mAxVoice[i][j];

            if (pAxVoice != NULL) {
                pAxVoice->SetVoiceType(type);
            }
        }
    }
}

void Voice::CalcAxSrc(bool initial) {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < mVoiceOutCount; i++) {
        f32 ratio = mPitch * ut::Clamp(mVoiceOutParam[i].pitch, 0.0f, 1.0f);

        for (int j = 0; j < mChannelCount; j++) {
            AxVoice* pAxVoice = mAxVoice[j][i];

            if (pAxVoice != NULL) {
                pAxVoice->SetSrc(ratio, initial);
            }
        }
    }
}

bool Voice::CalcAxVe() {
    ut::AutoInterruptLock lock;

    bool nextUpdate = false;

    f32 volume = 1.0f;
    volume *= mVolume;
    volume *= AxManager::GetInstance().GetOutputVolume();

    for (int i = 0; i < mVoiceOutCount; i++) {
        f32 voiceVolume = volume * mVoiceOutParam[i].volume;

        for (int j = 0; j < mChannelCount; j++) {
            AxVoice* pAxVoice = mAxVoice[j][i];

            if (pAxVoice != NULL) {
                nextUpdate = pAxVoice->SetVe(voiceVolume * mVeTargetVolume,
                                            voiceVolume * mVeInitVolume);
            }
        }
    }

    return nextUpdate;
}

bool Voice::CalcAxMix() {
    ut::AutoInterruptLock lock;

    bool nextUpdate = false;

    for (int i = 0; i < mChannelCount; i++) {
        for (int j = 0; j < mVoiceOutCount; j++) {
            AxVoice* pAxVoice = mAxVoice[i][j];

            if (pAxVoice != NULL) {
                AxVoice::MixParam mix;
                CalcMixParam(i, j, &mix);
                nextUpdate |= pAxVoice->SetMix(mix);

                if (mOutputLineFlag == 0 || mOutputLineFlag == OUTPUT_LINE_MAIN) {
                    pAxVoice->DisableRemote();
                } else {
                    AXPBRMTMIX rmtMix;
                    CalcRemoteMixParam(i, j, &rmtMix);

                    pAxVoice->EnableRemote();
                    pAxVoice->SetRmtMix(rmtMix);
                }
            }
        }
    }

    return nextUpdate;
}

void Voice::CalcAxLpf() {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < mVoiceOutCount; i++) {
        int freq = Util::CalcLpfFreq(mLpfFreq + mVoiceOutParam[i].lpf);

        for (int j = 0; j < mChannelCount; j++) {
            AxVoice* pAxVoice = mAxVoice[j][i];

            if (pAxVoice != NULL) {
                pAxVoice->SetLpf(freq);
            }
        }
    }
}

void Voice::AxVoiceCallbackFunc(AxVoice* pDropVoice,
                                AxVoice::AxVoiceCallbackStatus status,
                                void* pCallbackArg) {
    ut::AutoInterruptLock lock;

    Voice* p = static_cast<Voice*>(pCallbackArg);

    VoiceCallbackStatus voiceStatus;
    bool freeDropVoice = false;

    switch (status) {
    case AxVoice::CALLBACK_STATUS_CANCEL: {
        voiceStatus = CALLBACK_STATUS_CANCEL;
        break;
    }

    case AxVoice::CALLBACK_STATUS_DROP_DSP: {
        voiceStatus = CALLBACK_STATUS_DROP_DSP;
        freeDropVoice = true;
        break;
    }
    }

    for (int i = 0; i < p->mChannelCount; i++) {
        for (int j = 0; j < p->mVoiceOutCount; j++) {
            AxVoice* pAxVoice = p->mAxVoice[i][j];

            if (pAxVoice != NULL) {
                if (pAxVoice == pDropVoice) {
                    if (!freeDropVoice) {
                        AxVoiceManager::GetInstance().FreeAxVoice(pAxVoice);
                    }
                } else {
                    pAxVoice->Stop();
                    AxVoiceManager::GetInstance().FreeAxVoice(pAxVoice);
                }

                p->mAxVoice[i][j] = NULL;
            }
        }
    }

    p->mIsPause = false;
    p->mIsStarting = false;
    p->mChannelCount = 0;

    if (freeDropVoice) {
        p->Free();
    }

    if (p->mCallback != NULL) {
        p->mCallback(p, voiceStatus, p->mCallbackArg);
    }
}

void Voice::TransformDpl2Pan(f32* pPan, f32* pSurroundPan, f32 pan,
                             f32 surroundPan) {
    ut::AutoInterruptLock lock;

    surroundPan -= 1.0f;

    if (ut::Abs(pan) <= ut::Abs(surroundPan)) {
        if (surroundPan <= 0.0f) {
            *pPan = pan;
            *pSurroundPan = -0.12f + 0.88f * surroundPan;
        } else {
            *pPan = 0.5f * pan;
            *pSurroundPan = -0.12f + 1.12f * surroundPan;
        }
    } else if (pan >= 0.0f) {
        if (surroundPan <= 0.0f) {
            *pPan =
                (0.85f + (1.0f - 0.85f) * (-surroundPan / pan)) * ut::Abs(pan);
            *pSurroundPan = -0.12f + (2.0f * surroundPan + 0.88f * pan);
        } else {
            *pPan =
                (0.85f + (1.0f - 0.65f) * (-surroundPan / pan)) * ut::Abs(pan);
            *pSurroundPan = -0.12f + 1.12f * pan;
        }
    } else if (surroundPan <= 0.0f) {
        *pPan = ((1.0f - 0.85f) * (-surroundPan / pan) - 0.85f) * ut::Abs(pan);
        *pSurroundPan = -0.12f + (2.0f * surroundPan - 1.12f * pan);
    } else {
        *pPan = ((1.0f - 0.65f) * (-surroundPan / pan) - 0.85f) * ut::Abs(pan);
        *pSurroundPan = -0.12f + 1.12f * -pan;
    }

    *pSurroundPan += 1.0f;
}

void Voice::CalcMixParam(int channel, int voice, AxVoice::MixParam* pMix) {
    ut::AutoInterruptLock lock;

    f32 m_l, m_r, m_s;
    f32 a_l, a_r, a_s;
    f32 b_l, b_r, b_s;
    f32 c_l, c_r, c_s;

    f32& m_sl = m_s;
    f32& m_sr = c_l;

    f32& a_sl = a_s;
    f32& a_sr = c_r;

    f32& b_sl = b_s;
    f32& b_sr = c_s;

    m_l = 1.0f;
    m_r = 1.0f;
    m_s = 1.0f;
    a_l = 1.0f;
    a_r = 1.0f;
    a_s = 1.0f;
    b_l = 1.0f;
    b_r = 1.0f;
    b_s = 1.0f;
    c_l = 1.0f;
    c_r = 1.0f;
    c_s = 1.0f;

    switch (AxManager::GetInstance().GetOutputMode()) {
    case OUTPUT_MODE_STEREO:
    case OUTPUT_MODE_MONO: {
        m_s = 0.0f;
        a_s = 0.0f;
        b_s = 0.0f;
        c_s = 0.0f;
        break;
    }
    }

    f32 mainVolume = 1.0f;
    if (mOutputLineFlag & OUTPUT_LINE_MAIN) {
        mainVolume *= mMainOutVolume;
    } else {
        mainVolume = 0.0f;
    }

    switch (AxManager::GetInstance().GetOutputMode()) {
    case OUTPUT_MODE_STEREO:
    case OUTPUT_MODE_MONO: {
        m_l *= mainVolume;
        m_r *= mainVolume;
        a_l *= mainVolume;
        a_r *= mainVolume;
        b_l *= mainVolume;
        b_r *= mainVolume;
        c_l *= mainVolume;
        c_r *= mainVolume;
        break;
    }

    case OUTPUT_MODE_SURROUND: {
        m_l *= mainVolume;
        m_r *= mainVolume;
        m_s *= mainVolume;
        a_l *= mainVolume;
        a_r *= mainVolume;
        a_s *= mainVolume;
        b_l *= mainVolume;
        b_r *= mainVolume;
        b_s *= mainVolume;
        c_l *= mainVolume;
        c_r *= mainVolume;
        c_s *= mainVolume;
        break;
    }

    case OUTPUT_MODE_DPL2: {
        m_l *= mainVolume;
        m_r *= mainVolume;
        m_sl *= mainVolume;
        m_sr *= mainVolume;
        a_l *= mainVolume;
        a_r *= mainVolume;
        a_sl *= mainVolume;
        a_sr *= mainVolume;
        b_l *= mainVolume;
        b_r *= mainVolume;
        b_sl *= mainVolume;
        b_sr *= mainVolume;
        break;
    }
    }

    f32 voicePan = 0.0f;
    if (mChannelCount == 2) {
        if (channel == 0) {
            voicePan = -1.0f;
        }

        if (channel == 1) {
            voicePan = 1.0f;
        }
    }

    f32 pan, surroundPan;

    switch (AxManager::GetInstance().GetOutputMode()) {
    case OUTPUT_MODE_DPL2: {
        TransformDpl2Pan(
            &pan, &surroundPan,
            voicePan + (mPan + mAmbientPan) + mVoiceOutParam[voice].pan,
            mSurroundPan + mAmbientSurroundPan +
                mVoiceOutParam[voice].surroundPan);
        break;
    }

    case OUTPUT_MODE_MONO: {
        pan = 0.0f;
        surroundPan = 0.0f;
        break;
    }

    case OUTPUT_MODE_STEREO:
    case OUTPUT_MODE_SURROUND:
    default: {
        pan = voicePan + (mPan + mAmbientPan) + mVoiceOutParam[voice].pan;
        surroundPan = mSurroundPan + mAmbientSurroundPan +
                      mVoiceOutParam[voice].surroundPan;
        break;
    }
    }

    f32 left = Util::CalcPanRatio(pan);
    f32 right = Util::CalcPanRatio(-pan);
    f32 surround = Util::CalcVolumeRatio(-3.0f);

    switch (AxManager::GetInstance().GetOutputMode()) {
    case OUTPUT_MODE_STEREO:
    case OUTPUT_MODE_MONO: {
        m_l *= left;
        m_r *= right;
        a_l *= left;
        a_r *= right;
        b_l *= left;
        b_r *= right;
        c_l *= left;
        c_r *= right;
        break;
    }

    case OUTPUT_MODE_SURROUND: {
        m_l *= left;
        m_r *= right;
        m_s *= surround;
        a_l *= left;
        a_r *= right;
        a_s *= surround;
        b_l *= left;
        b_r *= right;
        b_s *= surround;
        c_l *= left;
        c_r *= right;
        c_s *= surround;
        break;
    }

    case OUTPUT_MODE_DPL2: {
        m_l *= left;
        m_r *= right;
        m_sl *= left;
        m_sr *= right;
        a_l *= left;
        a_r *= right;
        a_sl *= left;
        a_sr *= right;
        b_l *= left;
        b_r *= right;
        b_sl *= left;
        b_sr *= right;
        break;
    }
    }

    f32 front = Util::CalcSurroundPanRatio(surroundPan);
    f32 rear = Util::CalcSurroundPanRatio(2.0f - surroundPan);

    switch (AxManager::GetInstance().GetOutputMode()) {
    case OUTPUT_MODE_SURROUND: {
        m_l *= front;
        m_r *= front;
        m_s *= rear;
        a_l *= front;
        a_r *= front;
        a_s *= rear;
        b_l *= front;
        b_r *= front;
        b_s *= rear;
        c_l *= front;
        c_r *= front;
        c_s *= rear;
        break;
    }

    case OUTPUT_MODE_DPL2: {
        m_l *= front;
        m_r *= front;
        m_sl *= rear;
        m_sr *= rear;
        a_l *= front;
        a_r *= front;
        a_sl *= rear;
        a_sr *= rear;
        b_l *= front;
        b_r *= front;
        b_sl *= rear;
        b_sr *= rear;
        break;
    }

    case OUTPUT_MODE_STEREO:
    case OUTPUT_MODE_MONO:
    default: {
        break;
    }
    }

    f32 mainSend = mMainSend;
    f32 fxSendA =
        ut::Clamp(mFxSend[AUX_A] + mVoiceOutParam[voice].fxSend, 0.0f, 1.0f);
    f32 fxSendB = mFxSend[AUX_B];
    f32 fxSendC = mFxSend[AUX_C];

    switch (AxManager::GetInstance().GetOutputMode()) {
    case OUTPUT_MODE_STEREO:
    case OUTPUT_MODE_MONO: {
        m_l *= mainSend;
        m_r *= mainSend;
        a_l *= fxSendA;
        a_r *= fxSendA;
        b_l *= fxSendB;
        b_r *= fxSendB;
        c_l *= fxSendC;
        c_r *= fxSendC;
        break;
    }

    case OUTPUT_MODE_SURROUND: {
        m_l *= mainSend;
        m_r *= mainSend;
        m_s *= mainSend;
        a_l *= fxSendA;
        a_r *= fxSendA;
        a_s *= fxSendA;
        b_l *= fxSendB;
        b_r *= fxSendB;
        b_s *= fxSendB;
        c_l *= fxSendC;
        c_r *= fxSendC;
        c_s *= fxSendC;
        break;
    }

    case OUTPUT_MODE_DPL2: {
        m_l *= mainSend;
        m_r *= mainSend;
        m_sl *= mainSend;
        m_sr *= mainSend;
        a_l *= fxSendA;
        a_r *= fxSendA;
        a_sl *= fxSendA;
        a_sr *= fxSendA;
        b_l *= fxSendB;
        b_r *= fxSendB;
        b_sl *= fxSendB;
        b_sr *= fxSendB;
        break;
    }
    }

    pMix->vL = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * m_l);
    pMix->vR = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * m_r);
    pMix->vS = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * m_sl);

    pMix->vAuxAL = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * a_l);
    pMix->vAuxAR = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * a_r);
    pMix->vAuxAS = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * a_sl);

    pMix->vAuxBL = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * b_l);
    pMix->vAuxBR = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * b_r);
    pMix->vAuxBS = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * b_sl);

    pMix->vAuxCL = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * m_sr);
    pMix->vAuxCR = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * a_sr);
    pMix->vAuxCS = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * b_sr);
}

void Voice::CalcRemoteMixParam(int channel, int voice, AXPBRMTMIX* pMix) {
#pragma unused(channel)
#pragma unused(voice)

    ut::AutoInterruptLock lock;

    f32 main[WPAD_MAX_CONTROLLERS];
    f32 fx[WPAD_MAX_CONTROLLERS];

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        f32& rMain = main[i];
        f32& rFx = fx[i];

        rMain = 1.0f;
        rFx = 1.0f;

        f32 volume = 1.0f;
        if (mOutputLineFlag & (OUTPUT_LINE_REMOTE_N << i)) {
            volume *= mRemoteOutVolume[i];
        } else {
            volume = 0.0f;
        }

        rMain *= volume;
        rFx *= volume;

        f32 send = mRemoteSend[i];
        f32 fxSend = mRemoteFxSend[i];

        rMain *= send;
        rFx *= fxSend;
    }

    pMix->vMain0 = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * main[0]);
    pMix->vAux0 = 0;
    pMix->vMain1 = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * main[1]);
    pMix->vAux1 = 0;
    pMix->vMain2 = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * main[2]);
    pMix->vAux2 = 0;
    pMix->vMain3 = ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * main[3]);
    pMix->vAux3 = 0;

    pMix->vDeltaMain0 = 0;
    pMix->vDeltaAux0 = 0;
    pMix->vDeltaMain1 = 0;
    pMix->vDeltaAux1 = 0;
    pMix->vDeltaMain2 = 0;
    pMix->vDeltaAux2 = 0;
    pMix->vDeltaMain3 = 0;
    pMix->vDeltaAux3 = 0;
}

void Voice::InvalidateWaveData(const void* pStart, const void* pEnd) {
    ut::AutoInterruptLock lock;

    bool dispose = false;

    for (int i = 0; i < mChannelCount; i++) {
        AxVoice* pAxVoice = mAxVoice[i][0];

        if (pAxVoice != NULL && pAxVoice->IsAvailable() &&
            pAxVoice->IsDataAddressCoverd(pStart, pEnd)) {

            dispose = true;
            break;
        }
    }

    if (dispose && mCallback != NULL) {
        Stop();
        mCallback(this, CALLBACK_STATUS_CANCEL, mCallbackArg);
    }
}

} // namespace detail
} // namespace snd
} // namespace nw4r
