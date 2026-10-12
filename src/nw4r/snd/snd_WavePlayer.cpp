#include <nw4r/snd.h>
#include <nw4r/ut.h>

namespace nw4r {
namespace snd {
namespace detail {
namespace {

WavePlayerList sPlayerList;

} // namespace

void WavePlayer::Stop() {
    ut::AutoInterruptLock lock;

    if (mActiveFlag) {
        sPlayerList.Erase(this);
        mActiveFlag = false;
    }

    StopVoice();

    NW4R_UT_LINKLIST_FOREACH_SAFE (it, mWaveBufferList, {
        it->busyFlag = false;
        mWaveBufferList.Erase(it);

        if (mCallback != NULL) {
            mCallback(CALLBACK_STATUS_CANCEL, this, &*it, mCallbackArg);
        }
    })

    mPauseFlag = false;
}

void WavePlayer::StartVoice() {
    ut::AutoInterruptLock lock;

    WaveBuffer* pBuffer = &mWaveBufferList.GetFront();

    WaveData waveData;
    AxVoice::Format format;

    switch (mSampleFormat) {
    case SAMPLE_FORMAT_PCM16: {
        waveData.sampleFormat = WaveFile::FORMAT_PCM16;
        format = AxVoice::FORMAT_PCM16;
        break;
    }

    case SAMPLE_FORMAT_PCM8: {
        waveData.sampleFormat = WaveFile::FORMAT_PCM8;
        format = AxVoice::FORMAT_PCM8;
        break;
    }

    default: {
        return;
    }
    }

    waveData.loopFlag = false;
    waveData.numChannels = mChannelCount;
    waveData.sampleRate = static_cast<u16>(mSampleRate);
    waveData.loopStart = 0;
    waveData.loopEnd = AxVoice::GetSampleByByte(pBuffer->bufferSize, format);

    for (int i = 0; i < mChannelCount; i++) {
        waveData.channelParam[i].dataAddr = pBuffer->bufferAddress[i];
    }

    mVoice->Setup(waveData);
    mVoice->Start();
    mVoice->Pause(mPauseFlag);

    mStartedFlag = true;
    mLoopSetFlag = false;
}

void WavePlayer::SwitchBuffer() {
    WaveBuffer* pBuffer = &mWaveBufferList.GetFront();
    pBuffer->busyFlag = false;
    mWaveBufferList.PopFront();

    u32 samples = pBuffer->bufferSize;
    if (mSampleFormat == SAMPLE_FORMAT_PCM16) {
        samples /= sizeof(s16);
    }

    mPlaySampleCount += samples;

    if (mCallback != NULL) {
        mCallback(CALLBACK_STATUS_FINISH, this, pBuffer, mCallbackArg);
    }

    if (!mWaveBufferList.IsEmpty()) {
        WaveBuffer* pNext = &mWaveBufferList.GetFront();

        u32 endSample =
            AxVoice::GetSampleByByte(pNext->bufferSize, mVoice->GetFormat());

        for (int i = 0; i < mChannelCount; i++) {
            mVoice->SetBaseAddress(i, pNext->bufferAddress[i]);
            mVoice->StopAtPoint(i, pNext->bufferAddress[i], endSample);
        }
    }

    mLoopSetFlag = false;
}

inline void WavePlayer::UpdateBuffer() {
    if (mStartedFlag) {
        if (IsBufferEnd()) {
            SwitchBuffer();
        }

        if (!mLoopSetFlag && mWaveBufferList.GetSize() >= 2) {
            WaveBufferList::Iterator next = ++mWaveBufferList.GetBeginIter();

            for (int i = 0; i < mChannelCount; i++) {
                mVoice->SetLoopStart(i, next->bufferAddress[i], 0);
            }

            mVoice->SetLoopFlag(true);
            mLoopSetFlag = true;
        }
    } else if (!mWaveBufferList.IsEmpty()) {
        StartVoice();
    }
}

void WavePlayer::UpdateBufferAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->UpdateBuffer(); })
}

void WavePlayer::Update() {
    if (mVoice == NULL) {
        return;
    }

    mVoice->SetVolume(mVolume);
    mVoice->SetPitch(mPitch);
    mVoice->SetPan(mPan);
    mVoice->SetSurroundPan(mSurroundPan);
    mVoice->SetLpfFreq(1.0f + mLpfFreq);
    mVoice->SetOutputLine(mOutputLine);
    mVoice->SetMainOutVolume(mMainOutVolume);
    mVoice->SetMainSend(mMainSend);

    for (int i = 0; i < AUX_BUS_NUM; i++) {
        mVoice->SetFxSend(static_cast<AuxBus>(i), mFxSend[i]);
    }

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mVoice->SetRemoteOutVolume(i, mRemoteOutVolume[i]);
        mVoice->SetRemoteSend(i, mRemoteSend[i]);
        // @bug Applies the remote send, not the remote FX send
        mVoice->SetRemoteFxSend(i, mRemoteSend[i]);
    }
}

void WavePlayer::UpdateAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->Update(); })
}

void WavePlayer::StopAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->Stop(); })
}

} // namespace detail
} // namespace snd
} // namespace nw4r
