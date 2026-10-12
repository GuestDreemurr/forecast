#include <nw4r/snd.h>
#include <nw4r/ut.h>

#include <revolution/DVD.h>

#include <climits>

namespace nw4r {
namespace snd {
namespace detail {
namespace {

StrmPlayerList sPlayerList;

} // namespace

u8 StrmPlayer::sLoadBuffer[LOAD_BUFFER_SIZE] ALIGN(32);

/******************************************************************************
 *
 * Inline implementation
 *
 ******************************************************************************/
void StrmPlayer::UpdateLoopAddress(u32 startSample, u32 endSample) {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < mChannelCount; i++) {
        mVoice->SetLoopStart(i, mChannels[i].bufferAddress, startSample);
        mVoice->SetLoopEnd(i, mChannels[i].bufferAddress, endSample);
    }

    mVoice->SetLoopFlag(true);
}

void StrmPlayer::SetLoopEndToZeroBuffer(int endBlock) {
    {
        ut::AutoInterruptLock lock;

        for (int i = 0; i < mChannelCount; i++) {
            mVoice->StopAtPoint(i, mChannels[i].bufferAddress,
                                mStrmInfo.lastBlockSamples +
                                    (endBlock * mStrmInfo.blockSamples));
        }
    }

    mPlayFinishFlag = true;
}

void StrmPlayer::UpdateDataLoopAddress(s32 endBlock) {
    if (mStrmInfo.loopFlag) {
        s32 startBlock = endBlock + 1;
        if (startBlock >= mPlayingBufferBlockCount) {
            startBlock -= mPlayingBufferBlockCount;
        }

        ut::AutoInterruptLock lock;

        UpdateLoopAddress(startBlock * mStrmInfo.blockSamples,
                          mStrmInfo.lastBlockSamples +
                              (endBlock * mStrmInfo.blockSamples));

        if (mStrmInfo.format == WaveFile::FORMAT_ADPCM) {
            if (mVoice->GetFormat() == AxVoice::FORMAT_ADPCM) {
                mVoice->SetVoiceType(AxVoice::VOICE_TYPE_NORMAL);

                for (int i = 0; i < mChannelCount; i++) {
                    mVoice->SetAdpcmLoop(i, &mChannels[i].adpcmInfo.loopParam);
                }
            }

            if (endBlock == mPlayingBufferBlockCount - 1) {
                mSkipUpdateAdpcmLoop = true;
            }
        }

        return;
    }

    SetLoopEndToZeroBuffer(endBlock);
}

void StrmPlayer::UpdatePauseStatus() {
    ut::AutoInterruptLock lock;

    bool paused = false;

    if (mPauseFlag) {
        paused = true;
    }

    if (mLoadWaitFlag) {
        paused = true;
    }

    if (paused != mPauseStatus) {
        if (mVoice != NULL) {
            mVoice->Pause(paused);
        }

        mPauseStatus = paused;
    }
}

int StrmPlayer::CalcLoadingBufferBlockCount() const {
    int restBlocks = (mLastBlockIndex - mLoadingDataBlockIndex) + 1;
    int loopBlocks = (mLastBlockIndex - mLoopStartBlockIndex) + 1;

    if ((mBufferBlockCountBase + 1 - restBlocks) % loopBlocks == 0) {
        return mBufferBlockCountBase + 1;
    }

    return mBufferBlockCountBase;
}

StrmPlayer::StrmDataLoadCallback* StrmPlayer::AllocLoadCallback() {
    ut::AutoInterruptLock lock;

    StrmDataLoadCallback* pCallback = &mFreeLoadCallbackList.GetFront();
    mFreeLoadCallbackList.PopFront();

    return pCallback;
}

/******************************************************************************
 *
 * StrmPlayer
 *
 ******************************************************************************/
StrmPlayer::StrmPlayer() : mActiveFlag(false), mVoice(NULL) {
    for (u32 i = 0; i < DATA_BLOCK_COUNT_MAX; i++) {
        mLoadCallbackArea[i].mStrmPlayer = this;
        mFreeLoadCallbackList.PushBack(&mLoadCallbackArea[i]);
    }
}

bool StrmPlayer::Prepare(StrmBufferPool* pBufferPool,
                         StartOffsetType offsetType, int offset, int voices,
                         StrmLoader* pLoader, u32 userData) {
    if (mActiveFlag) {
        Shutdown();
    }

    InitParam(voices);

    mStartOffsetType = offsetType;
    mStartOffset = offset;
    mLoader = pLoader;
    mLoaderUserData = userData;
    mBufferPool = pBufferPool;

    switch (pLoader->LoadHeader(HeaderCallbackFunc, this, this, userData)) {
    case LOAD_RESULT_FINISHED: {
        break;
    }

    case LOAD_RESULT_FAILED: {
        Shutdown();
        break;
    }

    case LOAD_RESULT_CANCELED: {
        break;
    }

    case LOAD_RESULT_ASYNC: {
        break;
    }
    }

    mActiveFlag = true;
    return true;
}

bool StrmPlayer::Start() {
    ut::AutoInterruptLock lock;

    if (!mPreparedFlag) {
        return false;
    }

    if (mVoice == NULL) {
        return false;
    }

    if (!mStartedFlag) {
        mVoice->Start();
        UpdatePauseStatus();

        sPlayerList.PushBack(this);
        mStartedFlag = true;
    }

    return true;
}

void StrmPlayer::Stop() {
    if (mActiveFlag) {
        Shutdown();
    }
}

void StrmPlayer::Pause(bool flag) {
    mPauseFlag = flag;

    if (flag) {
        mLoadWaitFlag = true;
    }

    if (mStartedFlag) {
        UpdatePauseStatus();
    }
}

void StrmPlayer::UpdateBuffer() {
    if (!mStartedFlag) {
        return;
    }

    if (mVoice == NULL) {
        return;
    }

    s32 status = DVDGetDriveStatus();

    if (status == DVD_STATE_IDLE) {
        mDiskErrorFlag = false;
        UpdatePauseStatus();
    } else if (status != DVD_STATE_BUSY) {
        mDiskErrorFlag = true;
        mLoadWaitFlag = true;
        UpdatePauseStatus();
    }

    if (mLoadWaitFlag && mLoadCallbackList.IsEmpty()) {
        mLoadWaitFlag = false;
        UpdatePauseStatus();
    }

    if (!mPlayFinishFlag && !mNoRealtimeLoadFlag && !mLoadWaitFlag) {
        u32 sample = mVoice->GetCurrentPlayingSample();
        int block = sample / mStrmInfo.blockSamples;

        while (mPlayingBufferBlockIndex != block) {
            if (!mLoadWaitFlag &&
                mLoadCallbackList.GetSize() >= mBufferBlockCountBase - 2) {

                mLoadWaitFlag = true;
                UpdatePauseStatus();
                break;
            }

            UpdatePlayingBlockIndex();
            UpdateLoadingBlockIndex(true);
        }
    }
}

void StrmPlayer::Update() {
    if (!mStartedFlag) {
        return;
    }

    if (mVoice == NULL) {
        Shutdown();
        return;
    }

    ut::AutoInterruptLock lock;

    f32 volume = 1.0f;
    volume *= GetVolume();

    f32 pitchRatio = 1.0f;
    pitchRatio *= GetPitch();

    f32 pan = 0.0f;
    pan += GetPan();

    f32 surroundPan = 0.0f;
    surroundPan += GetSurroundPan();

    f32 ambientPan = 0.0f;
    ambientPan += GetAmbientPan();

    f32 ambientSurroundPan = 0.0f;
    ambientSurroundPan += GetAmbientSurroundPan();

    f32 lpfFreq = 1.0f;
    lpfFreq += GetLpfFreq();

    f32 mainSend = 0.0f;
    mainSend += GetMainSend();

    f32 fxSend[AUX_BUS_NUM];
    for (int i = 0; i < AUX_BUS_NUM; i++) {
        fxSend[i] = 0.0f;
        fxSend[i] += GetFxSend(static_cast<AuxBus>(i));
    }

    f32 remoteOutVolume[WPAD_MAX_CONTROLLERS];
    f32 remoteSend[WPAD_MAX_CONTROLLERS];
    f32 remoteFxSend[WPAD_MAX_CONTROLLERS];
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        remoteOutVolume[i] = GetRemoteOutVolume(i);

        remoteSend[i] = 0.0f;
        remoteSend[i] += GetRemoteSend(i);

        remoteFxSend[i] = 0.0f;
        remoteFxSend[i] += GetRemoteFxSend(i);
    }

    mVoice->SetVolume(volume);
    mVoice->SetPitch(pitchRatio);
    mVoice->SetPan(pan);
    mVoice->SetSurroundPan(surroundPan);
    mVoice->SetAmbientPan(ambientPan);
    mVoice->SetAmbientSurroundPan(ambientSurroundPan);
    mVoice->SetLpfFreq(lpfFreq);
    mVoice->SetOutputLine(GetOutputLine());
    mVoice->SetMainOutVolume(GetMainOutVolume());
    mVoice->SetMainSend(mainSend);

    for (int i = 0; i < AUX_BUS_NUM; i++) {
        mVoice->SetFxSend(static_cast<AuxBus>(i), fxSend[i]);
    }

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mVoice->SetRemoteOutVolume(i, remoteOutVolume[i]);
        mVoice->SetRemoteSend(i, remoteSend[i]);
        mVoice->SetRemoteFxSend(i, remoteFxSend[i]);
    }
}

void StrmPlayer::UpdatePlayingBlockIndex() {
    mPlayingDataBlockIndex++;

    if (mPlayingDataBlockIndex > mLastBlockIndex && mStrmInfo.loopFlag) {
        mPlayingDataBlockIndex = mLoopStartBlockIndex;

        if (mLoopCounter < INT_MAX) {
            mLoopCounter++;
        }

        UpdateLoopAddress(0, mPlayingBufferBlockCount * mStrmInfo.blockSamples);
    }

    mPlayingBufferBlockIndex++;

    if (mPlayingBufferBlockIndex >= mPlayingBufferBlockCount) {
        mPlayingBufferBlockIndex = 0;
        mPlayingBufferBlockCount = mLoadingBufferBlockCount;

        UpdateLoopAddress(0, mPlayingBufferBlockCount * mStrmInfo.blockSamples);
    }

    if (mPlayingBufferBlockIndex == mPlayingBufferBlockCount - 1 &&
        mVoice->GetFormat() == AxVoice::FORMAT_ADPCM) {

        if (!mSkipUpdateAdpcmLoop && mValidAdpcmLoop) {
            ut::AutoInterruptLock lock;

            for (int i = 0; i < mChannelCount; i++) {
                AdpcmLoopParam loopParam;
                loopParam.loop_pred_scale = mAdpcmLoopPredScale[i];
                loopParam.loop_yn1 = 0;
                loopParam.loop_yn2 = 0;

                mVoice->SetAdpcmLoop(i, &loopParam);
            }

            mVoice->SetVoiceType(AxVoice::VOICE_TYPE_STREAM);
        }

        mValidAdpcmLoop = false;
        mSkipUpdateAdpcmLoop = false;
    }

    if (mPlayingDataBlockIndex == mLastBlockIndex - 1) {
        UpdateDataLoopAddress(mPlayingBufferBlockIndex + 1);
    }
}

void StrmPlayer::UpdateLoadingBlockIndex(bool started) {
    if (mLoadFinishFlag) {
        return;
    }

    StrmDataLoadCallback* pCallback = AllocLoadCallback();
    pCallback->mStartedFlag = started;
    pCallback->mBufferBlockIndex = mLoadingBufferBlockIndex;
    pCallback->mDataBlockIndex = mLoadingDataBlockIndex;

    u32 blockSize =
        mLoadingDataBlockIndex < static_cast<s32>(mStrmInfo.numBlocks - 1)
            ? mStrmInfo.blockSize
            : mStrmInfo.lastBlockPaddedSize;

    u32 loadSize = mStrmInfo.blockHeaderOffset +
                   mChannelCount * ut::RoundUp(blockSize, 32);

    s32 loadOffset =
        mStrmInfo.dataOffset +
        mLoadingDataBlockIndex * (mStrmInfo.blockHeaderOffset +
                                  mStrmInfo.blockSize * mStrmInfo.numChannels);

    mLoadCallbackList.PushBack(pCallback);

    bool needUpdateAdpcmLoop = mLoadingBufferBlockIndex == 0 &&
                               mStrmInfo.format == WaveFile::FORMAT_ADPCM;

    switch (mLoader->LoadData(sLoadBuffer, loadSize, loadOffset, mChannelCount,
                              blockSize, mStrmInfo.blockHeaderOffset,
                              needUpdateAdpcmLoop, pCallback, this,
                              mLoaderUserData)) {
    case LOAD_RESULT_FINISHED: {
        pCallback->OnLoad(true);
        break;
    }

    case LOAD_RESULT_FAILED: {
        Shutdown();
        break;
    }

    case LOAD_RESULT_CANCELED: {
        break;
    }

    case LOAD_RESULT_ASYNC: {
        break;
    }
    }

    mLoadingDataBlockIndex++;

    if (mLoadingDataBlockIndex > mLastBlockIndex) {
        if (mStrmInfo.loopFlag) {
            mLoadingDataBlockIndex = mLoopStartBlockIndex;
        } else {
            mLoadFinishFlag = true;
            return;
        }
    }

    mLoadingBufferBlockIndex++;

    if (mLoadingBufferBlockIndex >= mLoadingBufferBlockCount) {
        mLoadingBufferBlockIndex = 0;
        mLoadingBufferBlockCount = CalcLoadingBufferBlockCount();
    }
}

void StrmPlayer::UpdateBufferAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->UpdateBuffer(); })
}

void StrmPlayer::UpdateAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->Update(); })
}

void StrmPlayer::StopAllPlayers() {
    NW4R_UT_LINKLIST_FOREACH_SAFE (it, sPlayerList, { it->Stop(); })
}

void StrmPlayer::InitParam(int voices) {
    BasicPlayer::InitParam();

    mActiveFlag = false;
    mStartedFlag = false;
    mPreparedFlag = false;
    mLoadFinishFlag = false;

    mPauseFlag = false;
    mDiskErrorFlag = false;
    mPauseStatus = false;

    mLoadWaitFlag = false;
    mNoRealtimeLoadFlag = false;

    mPlayFinishFlag = false;

    mSkipUpdateAdpcmLoop = false;
    mValidAdpcmLoop = false;

    mChannelCount = 0;
    mLoopCounter = 0;
    mVoiceOutCount = voices;

    for (int i = 0; i < CHANNEL_MAX; i++) {
        mChannels[i].bufferAddress = NULL;
        mChannels[i].bufferSize = 0;
    }
}

void StrmPlayer::OnHeaderLoad(const StrmHeader* pStrmHeader) {
    if (!SetupPlayer(pStrmHeader)) {
        Shutdown();
        return;
    }

    mPrepareCounter = 0;
    for (int i = 0; i < mBufferBlockCountBase; i++) {
        UpdateLoadingBlockIndex(false);
        mPrepareCounter++;

        if (mLoadFinishFlag) {
            break;
        }
    }

    if (mStrmInfo.numBlocks <= 2 && !mStrmInfo.loopFlag) {
        SetLoopEndToZeroBuffer(mStrmInfo.numBlocks - 1);
    }
}

bool StrmPlayer::SetupPlayer(const StrmHeader* pStrmHeader) {
    u32 poolBlockSize = mBufferPool->GetBlockSize();
    mStrmInfo = pStrmHeader->strmInfo;

    if (mStrmInfo.format == WaveFile::FORMAT_ADPCM) {
        for (int i = 0; i < mStrmInfo.numChannels; i++) {
            mChannels[i].adpcmInfo = pStrmHeader->adpcmInfo[i];
        }
    }

    mLoopStartBlockIndex = mStrmInfo.loopStart / mStrmInfo.blockSamples;
    mLastBlockIndex = mStrmInfo.numBlocks - 1;

    mDataBlockSize = mStrmInfo.blockSize;
    if (mDataBlockSize > DATA_BLOCK_SIZE_MAX) {
        return false;
    }

    mBufferBlockCount = poolBlockSize / mDataBlockSize;
    if (mBufferBlockCount < DATA_BLOCK_COUNT_MIN) {
        return false;
    } else if (mBufferBlockCount > DATA_BLOCK_COUNT_MAX) {
        mBufferBlockCount = DATA_BLOCK_COUNT_MAX;
    }

    mBufferBlockCountBase = mBufferBlockCount - 1;
    mChangeNumBlocks = mBufferBlockCountBase;

    s32 blockIndex = 0;
    if (mStrmInfo.blockSamples != 0) {
        if (mStartOffsetType == START_OFFSET_TYPE_SAMPLE) {
            blockIndex =
                mStartOffset / static_cast<s32>(mStrmInfo.blockSamples);
        } else if (mStartOffsetType == START_OFFSET_TYPE_MILLISEC) {
            blockIndex = (mStrmInfo.sampleRate * mStartOffset / 1000) /
                         static_cast<s32>(mStrmInfo.blockSamples);
        }
    }

    mPlayingDataBlockIndex = blockIndex;
    mLoadingDataBlockIndex = blockIndex;

    mLoadingBufferBlockIndex = 0;
    mPlayingBufferBlockIndex = 0;

    if (mNoRealtimeLoadFlag) {
        mLoadingBufferBlockCount = mStrmInfo.numBlocks;
    } else {
        mLoadingBufferBlockCount = CalcLoadingBufferBlockCount();
    }

    mPlayingBufferBlockCount = mLoadingBufferBlockCount;

    {
        ut::AutoInterruptLock lock;

        mChannelCount = ut::Min<int>(mStrmInfo.numChannels, CHANNEL_MAX);
        if (!AllocChannels(mChannelCount, mVoiceOutCount)) {
            return false;
        }
    }

    ut::AutoInterruptLock lock;

    WaveData waveData;
    waveData.sampleFormat = mStrmInfo.format;
    waveData.loopFlag = true;
    waveData.numChannels = mChannelCount;
    waveData.sampleRate = mStrmInfo.sampleRate;
    waveData.loopStart = 0;

    AxVoice::Format format =
        WaveFileReader::GetAxVoiceFormatFromWaveFileFormat(mStrmInfo.format);

    waveData.loopEnd = AxVoice::GetSampleByByte(
        mDataBlockSize * mPlayingBufferBlockCount, format);

    for (int i = 0; i < mChannelCount; i++) {
        ChannelParam& rParam = waveData.channelParam[i];

        rParam.dataAddr = mChannels[i].bufferAddress;
        rParam.adpcmInfo = mChannels[i].adpcmInfo;
    }

    mVoice->Setup(waveData);
    mVoice->SetVoiceType(AxVoice::VOICE_TYPE_STREAM);

    return true;
}

bool StrmPlayer::AllocChannels(int channels, int voices) {
    for (int i = 0; i < channels; i++) {
        void* pBuffer = mBufferPool->Alloc();

        if (pBuffer == NULL) {
            for (int j = 0; j < i; j++) {
                mBufferPool->Free(mChannels[j].bufferAddress);
            }

            return false;
        }

        mChannels[i].bufferAddress = pBuffer;
        mChannels[i].bufferSize = mBufferPool->GetBlockSize();
    }

    Voice* pVoice = VoiceManager::GetInstance().AllocVoice(
        channels, voices, Voice::PRIORITY_MAX, VoiceCallbackFunc, this);

    if (pVoice == NULL) {
        for (int i = 0; i < channels; i++) {
            mBufferPool->Free(mChannels[i].bufferAddress);
        }

        return false;
    }

    mVoice = pVoice;
    return true;
}

void StrmPlayer::FreeChannels() {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < mChannelCount; i++) {
        if (mChannels[i].bufferAddress != NULL) {
            mBufferPool->Free(mChannels[i].bufferAddress);

            mChannels[i].bufferAddress = NULL;
            mChannels[i].bufferSize = 0;
        }
    }

    mChannelCount = 0;

    if (mVoice != NULL) {
        mVoice->Free();
        mVoice = NULL;
    }
}

void StrmPlayer::Shutdown() {
    ut::AutoInterruptLock lock;

    if (mVoice != NULL) {
        mVoice->Stop();
    }

    if (mStartedFlag) {
        sPlayerList.Erase(this);

        mStartedFlag = false;
        mPreparedFlag = false;
    }

    mLoader->Cancel(this, mLoaderUserData);

    while (!mLoadCallbackList.IsEmpty()) {
        StrmDataLoadCallback* pCallback = &mLoadCallbackList.GetFront();

        mLoadCallbackList.PopFront();
        mFreeLoadCallbackList.PushBack(pCallback);
    }

    FreeChannels();
    mActiveFlag = false;
}

void StrmPlayer::HeaderCallbackFunc(bool success, const StrmHeader* pHeader,
                                    void* pCallbackArg) {
    StrmPlayer* pStrmPlayer = static_cast<StrmPlayer*>(pCallbackArg);

    if (success) {
        pStrmPlayer->OnHeaderLoad(pHeader);
    } else {
        pStrmPlayer->Shutdown();
    }
}

void StrmPlayer::VoiceCallbackFunc(Voice* pDropVoice,
                                   Voice::VoiceCallbackStatus status,
                                   void* pCallbackArg) {
    StrmPlayer* pStrmPlayer = static_cast<StrmPlayer*>(pCallbackArg);

    switch (status) {
    case Voice::CALLBACK_STATUS_FINISH_WAVE:
    case Voice::CALLBACK_STATUS_CANCEL: {
        pDropVoice->Free();
        pStrmPlayer->mVoice = NULL;
        break;
    }

    case Voice::CALLBACK_STATUS_DROP_VOICE:
    case Voice::CALLBACK_STATUS_DROP_DSP: {
        pStrmPlayer->mVoice = NULL;
        break;
    }

    default: {
        return;
    }
    }
}

/******************************************************************************
 *
 * StrmDataLoadCallback
 *
 ******************************************************************************/
void StrmPlayer::StrmDataLoadCallback::OnLoad(bool success) {
    ut::AutoInterruptLock lock;

    mStrmPlayer->mLoadCallbackList.Erase(this);

    if (success) {
        if (!mStartedFlag && --mStrmPlayer->mPrepareCounter == 0) {
            mStrmPlayer->mPreparedFlag = true;
        }
    } else {
        mStrmPlayer->Shutdown();
    }

    mStrmPlayer->mFreeLoadCallbackList.PushBack(this);
}

void StrmPlayer::StrmDataLoadCallback::SetAdpcmLoopContext(int channels,
                                                           u16* pPredScale) {
    if (mStrmPlayer->mStrmInfo.format != WaveFile::FORMAT_ADPCM) {
        return;
    }

    for (int i = 0; i < channels && i < CHANNEL_MAX; i++) {
        mStrmPlayer->mAdpcmLoopPredScale[i] = pPredScale[i];
    }

    mStrmPlayer->mValidAdpcmLoop = true;
}

void* StrmPlayer::StrmDataLoadCallback::GetBuffer(int channel) {
    if (channel >= mStrmPlayer->mChannelCount) {
        return NULL;
    }

    return static_cast<u8*>(mStrmPlayer->mChannels[channel].bufferAddress) +
           mStrmPlayer->mDataBlockSize * mBufferBlockIndex;
}

} // namespace detail
} // namespace snd
} // namespace nw4r
