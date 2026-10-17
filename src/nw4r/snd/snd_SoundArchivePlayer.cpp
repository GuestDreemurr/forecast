#include <nw4r/snd.h>
#include <nw4r/ut.h>

#include <revolution/DVD.h>

#include <cstring>

namespace nw4r {
namespace snd {

SoundArchivePlayer::SoundArchivePlayer()
    : mSoundArchive(NULL),
      mGroupTable(NULL),
      mSeqLoader(*this),
      mSeqCallback(*this),
      mWsdCallback(*this),
      mStrmLoader(*this),
      mSeqTrackAllocator(&mMmlSeqTrackAllocator),
      mFileManager(NULL),
      mSoundPlayerCount(0),
      mSoundPlayers(NULL),
      mMmlSeqTrackAllocator(&mMmlParser),
      mSetupBufferAddress(NULL),
      mSetupBufferSize(0) {

    detail::DisposeCallbackManager::GetInstance().RegisterDisposeCallback(this);
}

SoundArchivePlayer::~SoundArchivePlayer() {
    detail::DisposeCallbackManager::GetInstance().UnregisterDisposeCallback(
        this);
}

bool SoundArchivePlayer::IsAvailable() const {
    if (mSoundArchive == NULL) {
        return false;
    }

    return mSoundArchive->IsAvailable();
}

bool SoundArchivePlayer::Setup(const SoundArchive* pArchive, void* pMramBuffer,
                               u32 mramBufferSize, void* pStrmBuffer,
                               u32 strmBufferSize) {
    if (!SetupMram(pArchive, pMramBuffer, mramBufferSize)) {
        return false;
    }

    if (!SetupStrmBuffer(pArchive, pStrmBuffer, strmBufferSize)) {
        return false;
    }

    return true;
}

void SoundArchivePlayer::Shutdown() {
    mSoundArchive = NULL;
    mGroupTable = NULL;
    mFileManager = NULL;

    for (int i = 0; i < mSoundPlayerCount; i++) {
        mSoundPlayers[i].~SoundPlayer();
    }

    mSoundPlayerCount = 0;
    mSoundPlayers = NULL;

    mStrmBufferPool.Shutdown();

    mSeqSoundInstanceManager.Destroy(mSetupBufferAddress, mSetupBufferSize);
    mStrmSoundInstanceManager.Destroy(mSetupBufferAddress, mSetupBufferSize);
    mWaveSoundInstanceManager.Destroy(mSetupBufferAddress, mSetupBufferSize);
    mMmlSeqTrackAllocator.Destroy(mSetupBufferAddress, mSetupBufferSize);

    mSetupBufferAddress = NULL;
    mSetupBufferSize = 0;
}

u32 SoundArchivePlayer::GetRequiredMemSize(const SoundArchive* pArchive) {
    u32 size = 0;

    u32 playerCount = pArchive->GetPlayerCount();
    size += ut::RoundUp(playerCount * sizeof(SoundPlayer), 4);

    for (u32 i = 0; i < playerCount; i++) {
        SoundArchive::PlayerInfo info;
        if (!pArchive->ReadPlayerInfo(i, &info)) {
            continue;
        }

        for (int j = 0; j < info.playableSoundCount; j++) {
            if (info.heapSize == 0) {
                continue;
            }

            size += ut::RoundUp(info.heapSize + sizeof(detail::PlayerHeap), 4);
        }
    }

    // clang-format off
    size += ut::RoundUp(
        pArchive->GetGroupCount() * sizeof(Group) + (sizeof(GroupTable) - sizeof(Group)), 4);
    // clang-format on

    SoundArchive::SoundArchivePlayerInfo info;
    if (pArchive->ReadSoundArchivePlayerInfo(&info)) {
        // clang-format off
        size += ut::RoundUp(info.seqSoundCount  * sizeof(detail::SeqSound),    4);
        size += ut::RoundUp(info.strmSoundCount * sizeof(detail::StrmSound),   4);
        size += ut::RoundUp(info.waveSoundCount * sizeof(detail::WaveSound),   4);
        size += ut::RoundUp(info.seqTrackCount  * sizeof(detail::MmlSeqTrack), 4);
        // clang-format on
    }

    return size;
}

u32 SoundArchivePlayer::GetRequiredStrmBufferSize(
    const SoundArchive* pArchive) {
    int strmChannels = 0;

    SoundArchive::SoundArchivePlayerInfo info;
    if (pArchive->ReadSoundArchivePlayerInfo(&info)) {
        strmChannels = info.strmChannelCount;
    }

    // TODO(kiwi) How is this calculated?
    return strmChannels * 0xA000;
}

bool SoundArchivePlayer::SetupMram(const SoundArchive* pArchive, void* pBuffer,
                                   u32 bufferSize) {
    void* pEndPtr = static_cast<u8*>(pBuffer) + bufferSize;
    void* pPtr = pBuffer;

    if (!SetupSoundPlayer(pArchive, &pPtr, pEndPtr)) {
        return false;
    }

    if (!CreateGroupAddressTable(pArchive, &pPtr, pEndPtr)) {
        return false;
    }

    SoundArchive::SoundArchivePlayerInfo info;
    if (pArchive->ReadSoundArchivePlayerInfo(&info)) {
        if (!SetupSeqSound(pArchive, info.seqSoundCount, &pPtr, pEndPtr)) {
            return false;
        }

        if (!SetupStrmSound(pArchive, info.strmSoundCount, &pPtr, pEndPtr)) {
            return false;
        }

        if (!SetupWaveSound(pArchive, info.waveSoundCount, &pPtr, pEndPtr)) {
            return false;
        }

        if (!SetupSeqTrack(pArchive, info.seqTrackCount, &pPtr, pEndPtr)) {
            return false;
        }
    }

    mSoundArchive = pArchive;
    mSetupBufferAddress = pBuffer;
    mSetupBufferSize = bufferSize;

    return true;
}

detail::PlayerHeap* SoundArchivePlayer::CreatePlayerHeap(void* pBuffer,
                                                         u32 bufferSize) {
    detail::PlayerHeap* pHeap = new (pBuffer) detail::PlayerHeap();

    pBuffer = ut::AddOffsetToPtr(pBuffer, sizeof(detail::PlayerHeap));

    if (!pHeap->Create(pBuffer, bufferSize)) {
        return NULL;
    }

    return pHeap;
}

bool SoundArchivePlayer::SetupSoundPlayer(const SoundArchive* pArchive,
                                          void** ppBuffer, void* pEnd) {
    u32 playerCount = pArchive->GetPlayerCount();
    u32 requireSize = playerCount * sizeof(SoundPlayer);

    void* pPlayerEnd =
        ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

    if (ut::ComparePtr(pPlayerEnd, pEnd) > 0) {
        return false;
    }

    void* pPlayerBuffer = *ppBuffer;
    *ppBuffer = pPlayerEnd;

    mSoundPlayers = static_cast<SoundPlayer*>(pPlayerBuffer);
    mSoundPlayerCount = playerCount;

    u8* pPtr = static_cast<u8*>(pPlayerBuffer);

    for (u32 i = 0; i < playerCount; i++, pPtr += sizeof(SoundPlayer)) {
        SoundPlayer* pPlayer = new (pPtr) SoundPlayer();

        SoundArchive::PlayerInfo info;
        if (!pArchive->ReadPlayerInfo(i, &info)) {
            continue;
        }

        pPlayer->SetPlayableSoundCount(info.playableSoundCount);
        pPlayer->detail_SetPlayableSoundLimit(info.playableSoundCount);

        if (info.heapSize == 0) {
            continue;
        }

        for (int j = 0; j < info.playableSoundCount; j++) {
            u32 requireSize = sizeof(detail::PlayerHeap) + info.heapSize;

            void* pHeapEnd =
                ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

            if (ut::ComparePtr(pHeapEnd, pEnd) > 0) {
                return false;
            }

            void* pHeapBuffer = *ppBuffer;
            *ppBuffer = pHeapEnd;

            detail::PlayerHeap* pHeap =
                CreatePlayerHeap(pHeapBuffer, info.heapSize);

            if (pHeap == NULL) {
                return false;
            }

            pPlayer->detail_AppendPlayerHeap(pHeap);
        }
    }

    return true;
}

bool SoundArchivePlayer::CreateGroupAddressTable(const SoundArchive* pArchive,
                                                 void** ppBuffer, void* pEnd) {
    // clang-format off
    u32 requireSize = 
        pArchive->GetGroupCount() * sizeof(Group) + (sizeof(GroupTable) - sizeof(Group));
    // clang-format on

    void* pTableEnd =
        ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

    if (ut::ComparePtr(pTableEnd, pEnd) > 0) {
        return false;
    }

    mGroupTable = static_cast<GroupTable*>(*ppBuffer);
    *ppBuffer = pTableEnd;

    mGroupTable->count = pArchive->GetGroupCount();

    for (int i = 0; i < mGroupTable->count; i++) {
        mGroupTable->items[i].address = NULL;
        mGroupTable->items[i].waveDataAddress = NULL;
    }

    return true;
}

bool SoundArchivePlayer::SetupSeqSound(const SoundArchive* pArchive, int sounds,
                                       void** ppBuffer, void* pEnd) {
#pragma unused(pArchive)

    u32 requireSize = sounds * sizeof(detail::SeqSound);

    void* pSoundEnd =
        ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

    if (ut::ComparePtr(pSoundEnd, pEnd) > 0) {
        return false;
    }

    mSeqSoundInstanceManager.Create(*ppBuffer, requireSize);
    *ppBuffer = pSoundEnd;

    return true;
}

bool SoundArchivePlayer::SetupWaveSound(const SoundArchive* pArchive,
                                        int sounds, void** ppBuffer,
                                        void* pEnd) {
#pragma unused(pArchive)

    u32 requireSize = sounds * sizeof(detail::WaveSound);

    void* pSoundEnd =
        ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

    if (ut::ComparePtr(pSoundEnd, pEnd) > 0) {
        return false;
    }

    mWaveSoundInstanceManager.Create(*ppBuffer, requireSize);
    *ppBuffer = pSoundEnd;

    return true;
}

bool SoundArchivePlayer::SetupStrmSound(const SoundArchive* pArchive,
                                        int sounds, void** ppBuffer,
                                        void* pEnd) {
#pragma unused(pArchive)

    u32 requireSize = sounds * sizeof(detail::StrmSound);

    void* pSoundEnd =
        ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

    if (ut::ComparePtr(pSoundEnd, pEnd) > 0) {
        return false;
    }

    mStrmSoundInstanceManager.Create(*ppBuffer, requireSize);
    *ppBuffer = pSoundEnd;

    return true;
}

bool SoundArchivePlayer::SetupSeqTrack(const SoundArchive* pArchive, int tracks,
                                       void** ppBuffer, void* pEnd) {
#pragma unused(pArchive)

    u32 requireSize = tracks * sizeof(detail::MmlSeqTrack);

    void* pTrackEnd =
        ut::RoundUp(ut::AddOffsetToPtr(*ppBuffer, requireSize), 4);

    if (ut::ComparePtr(pTrackEnd, pEnd) > 0) {
        return false;
    }

    mMmlSeqTrackAllocator.Create(*ppBuffer, requireSize);
    *ppBuffer = pTrackEnd;

    return true;
}

bool SoundArchivePlayer::SetupStrmBuffer(const SoundArchive* pArchive,
                                         void* pBuffer, u32 bufferSize) {
    if (bufferSize < GetRequiredStrmBufferSize(pArchive)) {
        return false;
    }

    int strmChannels = 0;

    SoundArchive::SoundArchivePlayerInfo info;
    if (pArchive->ReadSoundArchivePlayerInfo(&info)) {
        strmChannels = info.strmChannelCount;
    }

    mStrmBufferPool.Setup(pBuffer, bufferSize, strmChannels);
    return true;
}

void SoundArchivePlayer::Update() {
    detail::SoundThread::AutoLock lock;

    for (u32 i = 0; i < mSoundPlayerCount; i++) {
        GetSoundPlayer(i).Update();
    }

    mSeqSoundInstanceManager.SortPriorityList();
    mStrmSoundInstanceManager.SortPriorityList();
    mWaveSoundInstanceManager.SortPriorityList();
}

const SoundArchive& SoundArchivePlayer::GetSoundArchive() const {
    return *mSoundArchive;
}

SoundPlayer& SoundArchivePlayer::GetSoundPlayer(u32 idx) {
    return mSoundPlayers[idx];
}

const void* SoundArchivePlayer::detail_GetFileAddress(u32 id) const {
    const void* pAddr = mSoundArchive->detail_GetFileAddress(id);
    if (pAddr != NULL) {
        return pAddr;
    }

    if (mFileManager != NULL) {
        const void* pAddr = mFileManager->GetFileAddress(id);
        if (pAddr != NULL) {
            return pAddr;
        }
    }

    SoundArchive::FileInfo file;
    if (!mSoundArchive->detail_ReadFileInfo(id, &file)) {
        return NULL;
    }

    for (unsigned int i = 0; i < file.filePosCount; i++) {
        SoundArchive::FilePos pos;
        if (!mSoundArchive->detail_ReadFilePos(id, i, &pos)) {
            continue;
        }

        const void* pGroup = GetGroupAddress(pos.groupId);
        if (pGroup == NULL) {
            continue;
        }

        SoundArchive::GroupItemInfo item;
        if (mSoundArchive->detail_ReadGroupItemInfo(pos.groupId, pos.index,
                                                    &item)) {
            return static_cast<const u8*>(pGroup) + item.offset;
        }
    }

    return NULL;
}

const void* SoundArchivePlayer::detail_GetFileWaveDataAddress(u32 id) const {
    const void* pAddr = mSoundArchive->detail_GetWaveDataFileAddress(id);
    if (pAddr != NULL) {
        return pAddr;
    }

    if (mFileManager != NULL) {
        const void* pAddr = mFileManager->GetFileWaveDataAddress(id);
        if (pAddr != NULL) {
            return pAddr;
        }
    }

    SoundArchive::FileInfo file;
    if (!mSoundArchive->detail_ReadFileInfo(id, &file)) {
        return NULL;
    }

    for (unsigned int i = 0; i < file.filePosCount; i++) {
        SoundArchive::FilePos pos;
        if (!mSoundArchive->detail_ReadFilePos(id, i, &pos)) {
            continue;
        }

        const void* pGroup = GetGroupWaveDataAddress(pos.groupId);
        if (pGroup == NULL) {
            continue;
        }

        SoundArchive::GroupItemInfo item;
        if (mSoundArchive->detail_ReadGroupItemInfo(pos.groupId, pos.index,
                                                    &item)) {
            return static_cast<const u8*>(pGroup) + item.waveDataOffset;
        }
    }

    return NULL;
}

const void* SoundArchivePlayer::GetGroupAddress(u32 id) const {
    if (mGroupTable == NULL) {
        return NULL;
    }

    if (id >= mGroupTable->count) {
        return NULL;
    }

    return mGroupTable->items[id].address;
}

void SoundArchivePlayer::SetGroupAddress(u32 id, const void* pAddr) {
    if (mGroupTable == NULL) {
        return;
    }

    // @bug Index not validated
    mGroupTable->items[id].address = pAddr;
}

const void* SoundArchivePlayer::GetGroupWaveDataAddress(u32 id) const {
    if (mGroupTable == NULL) {
        return NULL;
    }

    if (id >= mGroupTable->count) {
        return NULL;
    }

    return mGroupTable->items[id].waveDataAddress;
}

void SoundArchivePlayer::SetGroupWaveDataAddress(u32 id, const void* pAddr) {
    if (mGroupTable == NULL) {
        return;
    }

    // @bug Index not validated
    mGroupTable->items[id].waveDataAddress = pAddr;
}

SoundStartable::StartResult SoundArchivePlayer::PrepareStrmImpl(
    detail::StrmSound* pSound, const SoundArchive::SoundInfo* pSndInfo,
    const SoundArchive::StrmSoundInfo* pStrmInfo,
    SoundStartable::StartInfo::StartOffsetType startType, int startOffset,
    int voices) {

    // StrmSoundInfo is empty in this version of NW4R
#pragma unused(pStrmInfo)

    detail::StrmPlayer::StartOffsetType strmOffsetType =
        detail::StrmPlayer::START_OFFSET_TYPE_SAMPLE;

    if (startType == SoundStartable::StartInfo::START_OFFSET_TYPE_MILLISEC) {
        strmOffsetType = detail::StrmPlayer::START_OFFSET_TYPE_MILLISEC;
    }

    if (!pSound->Prepare(&mStrmBufferPool, strmOffsetType, startOffset, voices,
                         &mStrmLoader, pSndInfo->fileId)) {
        return SoundStartable::START_ERR_UNKNOWN;
    }

    pSound->SetInitialVolume(pSndInfo->volume / 127.0f);

    return SoundStartable::START_SUCCESS;
}

SoundStartable::StartResult SoundArchivePlayer::PrepareWaveSoundImpl(
    detail::WaveSound* pSound, const SoundArchive::SoundInfo* pSndInfo,
    const SoundArchive::WaveSoundInfo* pWsdInfo, int voices) {

    const void* pWsdBin = detail_GetFileAddress(pSndInfo->fileId);
    if (pWsdBin == NULL) {
        return SoundStartable::START_ERR_NOT_DATA_LOADED;
    }

    if (!pSound->Prepare(pWsdBin, pWsdInfo->subNo, voices, &mWsdCallback,
                         pSndInfo->fileId)) {
        return SoundStartable::START_ERR_UNKNOWN;
    }

    pSound->SetInitialVolume(pSndInfo->volume / 127.0f);
    pSound->SetChannelPriority(pWsdInfo->channelPriority);

    return SoundStartable::START_SUCCESS;
}

SoundStartable::StartResult SoundArchivePlayer::detail_SetupSound(
    SoundHandle* pHandle, u32 id, detail::BasicSound::AmbientArgInfo* pArgInfo,
    detail::ExternalSoundPlayer* pExtPlayer, bool hold,
    const StartInfo* pStartInfo) {

    if (!IsAvailable()) {
        return SoundStartable::START_ERR_NOT_AVAILABLE;
    }

    if (pHandle->IsAttachedSound()) {
        pHandle->DetachSound();
    }

    SoundArchive::SoundInfo sndInfo;
    if (!mSoundArchive->ReadSoundInfo(id, &sndInfo)) {
        return SoundStartable::START_ERR_INVALID_SOUNDID;
    }

    u32 playerId = sndInfo.playerId;
    int playerPriority = sndInfo.playerPriority;

    SoundStartable::StartInfo::StartOffsetType startType =
        SoundStartable::StartInfo::START_OFFSET_TYPE_MILLISEC;

    int startOffset = 0;
    int voices = 1;

    if (pStartInfo != NULL) {
        if (pStartInfo->playerId != SoundStartable::StartInfo::PLAYER_ID_DEFAULT) {
            playerId = pStartInfo->playerId;
        }

        if (pStartInfo->playerPriority >= 0) {
            playerPriority = pStartInfo->playerPriority;
        }

        startType = pStartInfo->startOffsetType;
        startOffset = pStartInfo->startOffset;
        voices = pStartInfo->voiceOutCount;
    }

    int playerPriorityStart = hold ? playerPriority - 1 : playerPriority;

    SoundPlayer& rPlayer = GetSoundPlayer(playerId);
    detail::BasicSound* pSound = NULL;

    switch (mSoundArchive->GetSoundType(id)) {
    case SOUND_TYPE_SEQ: {
        SoundArchive::SeqSoundInfo seqInfo;
        if (!mSoundArchive->detail_ReadSeqSoundInfo(id, &seqInfo)) {
            return SoundStartable::START_ERR_INVALID_SOUNDID;
        }

        detail::SeqSound* pSeqSound = rPlayer.detail_AllocSeqSound(
            playerPriority, playerPriorityStart, pArgInfo, pExtPlayer, id,
            &mSeqSoundInstanceManager);

        if (pSeqSound == NULL) {
            return SoundStartable::START_ERR_LOW_PRIORITY;
        }

        pSeqSound->SetId(id);

        SoundStartable::StartResult result =
            PrepareSeqImpl(pSeqSound, &sndInfo, &seqInfo, voices);

        if (result != SoundStartable::START_SUCCESS) {
            pSeqSound->Shutdown();
            return result;
        }

        pSound = pSeqSound;
        break;
    }

    case SOUND_TYPE_STRM: {
        SoundArchive::StrmSoundInfo strmInfo;
        if (!mSoundArchive->detail_ReadStrmSoundInfo(id, &strmInfo)) {
            return SoundStartable::START_ERR_INVALID_SOUNDID;
        }

        // Make sure the stream file exists before allocating the sound
        char streamBuffer[FILE_STREAM_BUFFER_SIZE];

        ut::FileStream* pFileStream = mSoundArchive->detail_OpenFileStream(
            sndInfo.fileId, streamBuffer, sizeof(streamBuffer));

        if (pFileStream == NULL) {
            return SoundStartable::START_ERR_CANNOT_OPEN_FILE;
        }

        pFileStream->Close();

        detail::StrmSound* pStrmSound = rPlayer.detail_AllocStrmSound(
            playerPriority, playerPriorityStart, pArgInfo, pExtPlayer, id,
            &mStrmSoundInstanceManager);

        if (pStrmSound == NULL) {
            return SoundStartable::START_ERR_LOW_PRIORITY;
        }

        pStrmSound->SetId(id);

        SoundStartable::StartResult result = PrepareStrmImpl(
            pStrmSound, &sndInfo, &strmInfo, startType, startOffset, voices);

        if (result != SoundStartable::START_SUCCESS) {
            pStrmSound->Shutdown();
            return result;
        }

        pSound = pStrmSound;
        break;
    }

    case SOUND_TYPE_WAVE: {
        SoundArchive::WaveSoundInfo waveInfo;
        if (!mSoundArchive->detail_ReadWaveSoundInfo(id, &waveInfo)) {
            return SoundStartable::START_ERR_INVALID_SOUNDID;
        }

        detail::WaveSound* pWaveSound = rPlayer.detail_AllocWaveSound(
            playerPriority, playerPriorityStart, pArgInfo, pExtPlayer, id,
            &mWaveSoundInstanceManager);

        if (pWaveSound == NULL) {
            return SoundStartable::START_ERR_LOW_PRIORITY;
        }

        pWaveSound->SetId(id);

        SoundStartable::StartResult result =
            PrepareWaveSoundImpl(pWaveSound, &sndInfo, &waveInfo, voices);

        if (result != SoundStartable::START_SUCCESS) {
            pWaveSound->Shutdown();
            return result;
        }

        pSound = pWaveSound;
        break;
    }

    default: {
        return SoundStartable::START_ERR_INVALID_SOUNDID;
    }
    }

    pHandle->detail_AttachSound(pSound);
    return SoundStartable::START_SUCCESS;
}

SoundStartable::StartResult SoundArchivePlayer::PrepareSeqImpl(
    detail::SeqSound* pSound, const SoundArchive::SoundInfo* pSndInfo,
    const SoundArchive::SeqSoundInfo* pSeqInfo, int voices) {

    const void* pSeqBin = detail_GetFileAddress(pSndInfo->fileId);

    if (pSeqBin == NULL) {
        // The sequence will be loaded into the player heap
        detail::PlayerHeap* pHeap = pSound->GetPlayerHeap();
        if (pHeap == NULL) {
            return SoundStartable::START_ERR_NOT_DATA_LOADED;
        }

        char streamBuffer[FILE_STREAM_BUFFER_SIZE];

        detail::FileStreamHandle stream(mSoundArchive->detail_OpenFileStream(
            pSndInfo->fileId, streamBuffer, sizeof(streamBuffer)));

        if (pHeap->GetFreeSize() < stream->GetSize()) {
            return SoundStartable::START_ERR_NOT_ENOUGH_PLAYER_HEAP;
        }
    }

    detail::SeqPlayer::SetupResult result = pSound->Setup(
        mSeqTrackAllocator, pSeqInfo->allocTrack, voices, &mSeqCallback);

    if (result != detail::SeqPlayer::SETUP_SUCCESS) {
        if (result == detail::SeqPlayer::SETUP_ERR_CANNOT_ALLOCATE_TRACK) {
            return SoundStartable::START_ERR_CANNOT_ALLOCATE_TRACK;
        }

        return SoundStartable::START_ERR_UNKNOWN;
    }

    pSound->SetInitialVolume(pSndInfo->volume / 127.0f);
    pSound->SetChannelPriority(pSeqInfo->channelPriority);

    if (pSeqBin != NULL) {
        detail::SeqFileReader reader(pSeqBin);
        pSound->Prepare(reader.GetBaseAddress(), pSeqInfo->dataOffset);
    } else {
        pSound->Prepare(&mSeqLoader, reinterpret_cast<u32>(pSound));
    }

    return SoundStartable::START_SUCCESS;
}

bool SoundArchivePlayer::LoadGroup(u32 id, SoundMemoryAllocatable* pAllocatable,
                                   u32 blockSize) {
    if (!IsAvailable()) {
        return false;
    }

    if (id >= mSoundArchive->GetGroupCount()) {
        return false;
    }

    if (GetGroupAddress(id) != NULL) {
        return true;
    }

    if (pAllocatable == NULL) {
        return false;
    }

    detail::SoundArchiveLoader loader(*mSoundArchive);

    void* pWaveBuffer;
    const void* pGroup =
        loader.LoadGroup(id, pAllocatable, &pWaveBuffer, blockSize);

    if (pGroup == NULL) {
        return NULL;
    }

    SetGroupAddress(id, pGroup);
    SetGroupWaveDataAddress(id, pWaveBuffer);

    return true;
}

bool SoundArchivePlayer::LoadGroup(const char* pLabel,
                                   SoundMemoryAllocatable* pAllocatable,
                                   u32 blockSize) {
    u32 id = mSoundArchive->ConvertLabelStringToGroupId(pLabel);
    if (id == SoundArchive::INVALID_ID) {
        return false;
    }

    return LoadGroup(id, pAllocatable, blockSize);
}

void SoundArchivePlayer::InvalidateData(const void* pStart, const void* pEnd) {
    if (mGroupTable == NULL) {
        return;
    }

    for (int i = 0; i < mGroupTable->count; i++) {
        const void* pAddr = mGroupTable->items[i].address;

        if (pStart <= pAddr && pAddr <= pEnd) {
            mGroupTable->items[i].address = NULL;
        }
    }
}

void SoundArchivePlayer::InvalidateWaveData(const void* pStart,
                                            const void* pEnd) {
    if (mGroupTable == NULL) {
        return;
    }

    for (int i = 0; i < mGroupTable->count; i++) {
        const void* pAddr = mGroupTable->items[i].waveDataAddress;

        if (pStart <= pAddr && pAddr <= pEnd) {
            mGroupTable->items[i].waveDataAddress = NULL;
        }
    }
}

/******************************************************************************
 *
 * SeqLoader
 *
 ******************************************************************************/
int SoundArchivePlayer::SeqLoader::LoadAsync(
    detail::SeqSound::SeqLoadCallback pCallback, void* pCallbackArg,
    u32 userData) {

    if (!mSoundArchivePlayer.IsAvailable()) {
        return detail::StrmPlayer::LOAD_RESULT_FAILED;
    }

    detail::SeqSound* pSound = reinterpret_cast<detail::SeqSound*>(userData);
    const SoundArchive& rArchive = mSoundArchivePlayer.GetSoundArchive();
    u32 id = pSound->GetId();

    SoundArchive::SoundInfo sndInfo;
    if (!rArchive.ReadSoundInfo(id, &sndInfo)) {
        return detail::StrmPlayer::LOAD_RESULT_FAILED;
    }

    SoundArchive::SeqSoundInfo seqInfo;
    if (!rArchive.detail_ReadSeqSoundInfo(id, &seqInfo)) {
        return detail::StrmPlayer::LOAD_RESULT_FAILED;
    }

    detail::PlayerHeap* pHeap = pSound->GetPlayerHeap();
    if (pHeap == NULL) {
        return detail::StrmPlayer::LOAD_RESULT_FAILED;
    }

    detail::Task* pTask = new (detail::TaskManager::GetInstance().Alloc())
        SeqLoadTask(userData, &rArchive, sndInfo.fileId, seqInfo.dataOffset,
                    pHeap, pCallback, pCallbackArg, &mMutex);

    detail::TaskManager::GetInstance().AppendTask(
        pTask, detail::TaskManager::PRIORITY_MIDDLE);

    detail::TaskThread::GetInstance().SendWakeupMessage();

    return detail::StrmPlayer::LOAD_RESULT_ASYNC;
}

void SoundArchivePlayer::SeqLoader::Cancel(u32 userData) {
    detail::TaskManager::GetInstance().CancelByTaskId(userData);
}

/******************************************************************************
 *
 * SeqNoteOnCallback
 *
 ******************************************************************************/
detail::Channel*
SoundArchivePlayer::SeqNoteOnCallback::NoteOn(detail::SeqPlayer* pSeqPlayer,
                                              int bankNo,
                                              const detail::NoteOnInfo& rInfo) {
#pragma unused(bankNo)

    if (!mSoundArchivePlayer.IsAvailable()) {
        return NULL;
    }

    const SoundArchive& rArchive = mSoundArchivePlayer.GetSoundArchive();
    u32 soundId = pSeqPlayer->GetId();

    SoundArchive::SeqSoundInfo seqInfo;
    if (!rArchive.detail_ReadSeqSoundInfo(soundId, &seqInfo)) {
        return NULL;
    }

    SoundArchive::BankInfo bankInfo;
    if (!rArchive.detail_ReadBankInfo(seqInfo.bankId, &bankInfo)) {
        return NULL;
    }

    const void* pBankBin =
        mSoundArchivePlayer.detail_GetFileAddress(bankInfo.fileId);

    if (pBankBin == NULL) {
        return NULL;
    }

    detail::Bank bank(pBankBin);

    const void* pWaveData =
        mSoundArchivePlayer.detail_GetFileWaveDataAddress(bankInfo.fileId);

    if (pWaveData == NULL) {
        return NULL;
    }

    bank.SetWaveDataAddress(pWaveData);
    return bank.NoteOn(rInfo);
}

bool SoundArchivePlayer::WsdCallback::GetWaveSoundData(
    detail::WaveSoundInfo* pSoundInfo, detail::WaveSoundNoteInfo* pNoteInfo,
    detail::WaveData* pWaveData, const void* pWsdData, int index, int noteIndex,
    u32 callbackArg) const {

    u32 fileId = callbackArg;

    if (!mSoundArchivePlayer.IsAvailable()) {
        return false;
    }

    const void* pWaveAddr =
        mSoundArchivePlayer.detail_GetFileWaveDataAddress(fileId);

    if (pWaveAddr == NULL) {
        return false;
    }

    detail::WsdFileReader reader(pWsdData);

    if (!reader.ReadWaveSoundInfo(pSoundInfo, index)) {
        return false;
    }

    if (!reader.ReadWaveSoundNoteInfo(pNoteInfo, index, noteIndex)) {
        return false;
    }

    if (!reader.ReadWaveParam(pNoteInfo->waveIndex, pWaveData, pWaveAddr)) {
        return false;
    }

    return true;
}

/******************************************************************************
 *
 * StrmLoader
 *
 ******************************************************************************/
detail::StrmPlayer::LoadResult SoundArchivePlayer::StrmLoader::LoadHeader(
    HeaderCallback pCallback, void* pCallbackArg, detail::StrmPlayer* pPlayer,
    u32 userData) {

    if (!mSoundArchivePlayer.IsAvailable()) {
        return detail::StrmPlayer::LOAD_RESULT_FAILED;
    }

    void* pBuffer = detail::TaskManager::GetInstance().Alloc();
    const SoundArchive& rArchive = mSoundArchivePlayer.GetSoundArchive();

    detail::Task* pTask = new (pBuffer)
        StrmHeaderLoadTask(reinterpret_cast<u32>(pPlayer), &rArchive, userData,
                           pCallback, pCallbackArg, &mMutex);

    detail::TaskManager::GetInstance().AppendTask(
        pTask, detail::TaskManager::PRIORITY_MIDDLE);

    detail::TaskThread::GetInstance().SendWakeupMessage();

    return detail::StrmPlayer::LOAD_RESULT_ASYNC;
}

detail::StrmPlayer::LoadResult SoundArchivePlayer::StrmLoader::LoadData(
    void* pBuffer, u32 size, s32 offset, int channels, u32 blockSize,
    u32 blockHeaderOffset, bool needUpdateAdpcmLoop,
    detail::StrmPlayer::StrmDataLoadCallback* pCallback,
    detail::StrmPlayer* pPlayer, u32 userData) {

    if (!mSoundArchivePlayer.IsAvailable()) {
        return detail::StrmPlayer::LOAD_RESULT_FAILED;
    }

    const SoundArchive& rArchive = mSoundArchivePlayer.GetSoundArchive();

    detail::Task* pTask = new (detail::TaskManager::GetInstance().Alloc())
        StrmDataLoadTask(pBuffer, size, offset, channels, blockSize,
                         blockHeaderOffset, needUpdateAdpcmLoop, pCallback,
                         &rArchive, userData, reinterpret_cast<u32>(pPlayer),
                         &mMutex);

    detail::TaskManager::GetInstance().AppendTask(
        pTask, detail::TaskManager::PRIORITY_HIGH);

    detail::TaskThread::GetInstance().SendWakeupMessage();

    return detail::StrmPlayer::LOAD_RESULT_ASYNC;
}

void SoundArchivePlayer::StrmLoader::Cancel(detail::StrmPlayer* pPlayer,
                                            u32 userData) {
#pragma unused(userData)

    detail::TaskManager::GetInstance().CancelByTaskId(
        reinterpret_cast<u32>(pPlayer));
}

/******************************************************************************
 *
 * SeqLoadTask
 *
 ******************************************************************************/
void SoundArchivePlayer::SeqLoadTask::Execute() {
    // The second loader reset locks the mutex by hand through this local
    OSMutex* pMutex;
    detail::SoundArchiveLoader loader(*mSoundArchive);

    SetLoader(&loader);
    const void* pSeqBin = loader.LoadFile(mFileId, mAllocatable);
    pMutex = mMutex;
    OSLockMutex(pMutex);
    mLoader = NULL;
    OSUnlockMutex(pMutex);

    if (pSeqBin == NULL) {
        Notify(false, NULL, 0);
        return;
    }

    detail::SeqFileReader reader(pSeqBin);
    const void* pBase = reader.GetBaseAddress();

    Notify(true, pBase, mDataOffset);
}

void SoundArchivePlayer::SeqLoadTask::Cancel() {
    mCallback = NULL;

    ut::detail::AutoLock<OSMutex> lock(*mMutex);

    if (mLoader != NULL) {
        mLoader->Cancel();
    }
}

/******************************************************************************
 *
 * StrmHeaderLoadTask
 *
 ******************************************************************************/
void SoundArchivePlayer::StrmHeaderLoadTask::Execute() {
    static u8 sHeaderBuffer[FILE_STREAM_BUFFER_SIZE] ALIGN(32);
    static OSMutex sMutex;
    static bool sInitialized = false;

    char streamBuffer[FILE_STREAM_BUFFER_SIZE];

    {
        ut::detail::AutoLock<OSMutex> lock(*mMutex);

        mFileStream = mSoundArchive->detail_OpenFileStream(
            mFileId, streamBuffer, sizeof(streamBuffer));
    }

    if (mFileStream == NULL) {
        Notify(false, NULL);

        return;
    }

    if (!mFileStream->CanSeek() || !mFileStream->CanRead()) {
        mFileStream->Close();
        mFileStream = NULL;

        Notify(false, NULL);

        return;
    }

    if (!sInitialized) {
        OSInitMutex(&sMutex);
        sInitialized = true;
    }

    ut::detail::AutoLock<OSMutex> lock(sMutex);

    detail::StrmFileLoader loader(*mFileStream);

    if (!loader.LoadFileHeader(sHeaderBuffer, sizeof(sHeaderBuffer))) {
        mFileStream->Close();
        mFileStream = NULL;

        Notify(false, NULL);

        return;
    }

    {
        ut::detail::AutoLock<OSMutex> lock(*mMutex);

        mFileStream->Close();
        mFileStream = NULL;
    }

    detail::StrmPlayer::StrmHeader header;
    loader.ReadStrmInfo(&header.strmInfo);

    for (int i = 0; i < header.strmInfo.numChannels; i++) {
        loader.ReadAdpcmInfo(&header.adpcmInfo[i], i);
    }

    Notify(true, &header);
}

void SoundArchivePlayer::StrmHeaderLoadTask::Cancel() {
    mCallback = NULL;

    ut::detail::AutoLock<OSMutex> lock(*mMutex);

    if (mFileStream != NULL && mFileStream->CanCancel()) {
        if (mFileStream->CanAsync()) {
            mFileStream->CancelAsync(NULL, NULL);
        } else {
            mFileStream->Cancel();
        }
    }
}

/******************************************************************************
 *
 * StrmDataLoadTask
 *
 ******************************************************************************/
SoundArchivePlayer::StrmDataLoadTask::StrmDataLoadTask(
    void* pBuffer, u32 size, s32 offset, int channels, u32 blockSize,
    u32 blockHeaderOffset, bool needUpdateAdpcmLoop,
    detail::StrmPlayer::StrmDataLoadCallback* pCallback,
    const SoundArchive* pArchive, u32 fileId, u32 taskId, OSMutex* pMutex)
    : detail::Task(taskId),
      mCallback(pCallback),
      mFileStream(NULL),
      mSoundArchive(pArchive),
      mFileId(fileId),
      mBuffer(pBuffer),
      mSize(size),
      mOffset(offset),
      mChannels(channels),
      mBlockSize(blockSize),
      mBlockHeaderOffset(blockHeaderOffset),
      mNeedUpdateAdpcmLoop(needUpdateAdpcmLoop),
      mMutex(pMutex) {}

void SoundArchivePlayer::StrmDataLoadTask::Execute() {
    DCInvalidateRange(mBuffer, mSize);

    char streamBuffer[FILE_STREAM_BUFFER_SIZE];

    {
        ut::detail::AutoLock<OSMutex> lock(*mMutex);

        mFileStream = mSoundArchive->detail_OpenFileStream(
            mFileId, streamBuffer, sizeof(streamBuffer));
    }

    if (mFileStream == NULL) {
        Notify(false);

        return;
    }

    if (!mFileStream->CanSeek() || !mFileStream->CanRead()) {
        mFileStream->Close();
        mFileStream = NULL;

        Notify(false);

        return;
    }

    ut::DvdFileStream* pDvdStream =
        ut::DynamicCast<ut::DvdFileStream*>(mFileStream);

    if (pDvdStream != NULL) {
        pDvdStream->SetPriority(DVD_PRIO_HIGH);
    }

    mFileStream->Seek(mOffset, ut::FileStream::SEEK_ORIGIN_BEG);
    s32 bytesRead = mFileStream->Read(mBuffer, mSize);

    {
        OSMutex* pMutex = mMutex;
        OSLockMutex(pMutex);

        mFileStream->Close();
        mFileStream = NULL;

        OSUnlockMutex(pMutex);
    }

    if (bytesRead == DVD_RESULT_CANCELED) {
        Notify(false);

        return;
    }

    if (bytesRead != mSize) {
        Notify(false);

        return;
    }

    u16 adpcmPredScale[CHANNEL_MAX];
    u8* pBuffer = static_cast<u8*>(mBuffer);

    for (int i = 0; i < mChannels; i++) {
        if (mNeedUpdateAdpcmLoop) {
            adpcmPredScale[i] = (pBuffer + mBlockHeaderOffset)[
                i * ut::RoundUp(mBlockSize, 32)];
        }

        if (mCallback != NULL) {
            const void* pSrc;
            u32 len = (mBlockSize + 31) & ~31;

            pSrc = static_cast<u8*>(mBuffer) + mBlockHeaderOffset + i * len;

            void* pDst = mCallback->GetBuffer(i);

            std::memcpy(pDst, pSrc, len);
            DCFlushRange(pDst, len);
        }
    }

    if (mNeedUpdateAdpcmLoop && mCallback != NULL) {
        mCallback->SetAdpcmLoopContext(mChannels, adpcmPredScale);
    }

    Notify(true);
}

void SoundArchivePlayer::StrmDataLoadTask::Cancel() {
    mCallback = NULL;

    ut::detail::AutoLock<OSMutex> lock(*mMutex);

    if (mFileStream != NULL && mFileStream->CanCancel()) {
        if (mFileStream->CanAsync()) {
            mFileStream->CancelAsync(NULL, NULL);
        } else {
            mFileStream->Cancel();
        }
    }
}

} // namespace snd
} // namespace nw4r
