#include <nw4r/snd.h>
#include <nw4r/ut.h>

namespace nw4r {
namespace snd {
namespace detail {

SoundArchiveLoader::SoundArchiveLoader(const SoundArchive& rArchive)
    : mArc(rArchive), mStream(NULL) {
    OSInitMutex(&mMutex);
}

SoundArchiveLoader::~SoundArchiveLoader() {}

void* SoundArchiveLoader::LoadGroup(u32 id,
                                    SoundMemoryAllocatable* pAllocatable,
                                    void** ppWaveBuffer, u32 blockSize) {
    ut::detail::AutoLock<OSMutex> lock(mMutex);

    FileStreamHandle groupHandle(
        mArc.detail_OpenGroupStream(id, mStreamArea, sizeof(mStreamArea)));

    if (!groupHandle) {
        return NULL;
    }

    if (!groupHandle->CanSeek() || !groupHandle->CanRead()) {
        return NULL;
    }

    void* pGroupBuffer = pAllocatable->Alloc(groupHandle->GetSize());
    if (pGroupBuffer == NULL) {
        return NULL;
    }

    mStream = groupHandle.GetFileStream();

    if (blockSize == 0) {
        s32 bytesRead = groupHandle->Read(pGroupBuffer, groupHandle->GetSize());

        if (bytesRead == 0) {
            mStream = NULL;
            return NULL;
        }
    } else {
        u8* pReadPtr = static_cast<u8*>(pGroupBuffer);
        u32 bytesLeft = groupHandle->GetSize();

        while (bytesLeft) {
            s32 bytesRead =
                groupHandle->Read(pReadPtr, ut::Min(blockSize, bytesLeft));

            if (bytesRead == 0) {
                mStream = NULL;
                return NULL;
            }

            if (bytesLeft > blockSize) {
                bytesLeft -= blockSize;
                pReadPtr += blockSize;
            } else {
                bytesLeft = 0;
            }
        }
    }

    mStream = NULL;

    SoundArchive::GroupInfo groupInfo;
    if (!mArc.detail_ReadGroupInfo(id, &groupInfo)) {
        return NULL;
    }

    if (groupInfo.waveDataSize != 0) {
        FileStreamHandle waveHandle(mArc.detail_OpenGroupWaveDataStream(
            id, mStreamArea, sizeof(mStreamArea)));

        if (!waveHandle) {
            return NULL;
        }

        if (!waveHandle->CanSeek() || !waveHandle->CanRead()) {
            return NULL;
        }

        void* pWaveBuffer = pAllocatable->Alloc(waveHandle->GetSize());
        if (pWaveBuffer == NULL) {
            return NULL;
        }

        mStream = waveHandle.GetFileStream();

        if (blockSize == 0) {
            s32 bytesRead =
                waveHandle->Read(pWaveBuffer, waveHandle->GetSize());

            if (bytesRead == 0) {
                mStream = NULL;
                return NULL;
            }
        } else {
            u8* pReadPtr = static_cast<u8*>(pWaveBuffer);
            u32 bytesLeft = waveHandle->GetSize();

            while (bytesLeft) {
                s32 bytesRead =
                    waveHandle->Read(pReadPtr, ut::Min(blockSize, bytesLeft));

                if (bytesRead == 0) {
                    mStream = NULL;
                    return NULL;
                }

                if (bytesLeft > blockSize) {
                    bytesLeft -= blockSize;
                    pReadPtr += blockSize;
                } else {
                    bytesLeft = 0;
                }
            }
        }

        mStream = NULL;

        if (ppWaveBuffer != NULL) {
            *ppWaveBuffer = pWaveBuffer;
        }
    } else if (ppWaveBuffer != NULL) {
        *ppWaveBuffer = NULL;
    }

    return pGroupBuffer;
}

s32 SoundArchiveLoader::ReadFile(u32 id, void* pBuffer, s32 size, s32 offset) {
    ut::detail::AutoLock<OSMutex> lock(mMutex);

    FileStreamHandle fileHandle(
        mArc.detail_OpenFileStream(id, mStreamArea, sizeof(mStreamArea)));

    if (!fileHandle) {
        return -1;
    }

    if (!fileHandle->CanSeek() || !fileHandle->CanRead()) {
        return -1;
    }

    fileHandle->Seek(offset, ut::FileStream::SEEK_ORIGIN_BEG);

    mStream = fileHandle.GetFileStream();
    s32 bytesRead = fileHandle->Read(pBuffer, ut::RoundUp(size, 32));
    mStream = NULL;

    if (bytesRead == 0) {
        return -1;
    }

    return size;
}

void* SoundArchiveLoader::LoadFile(u32 id,
                                   SoundMemoryAllocatable* pAllocatable) {
    SoundArchive::FileInfo fileInfo;
    if (!mArc.detail_ReadFileInfo(id, &fileInfo)) {
        return NULL;
    }

    u32 fileSize = fileInfo.fileSize;
    if (fileSize == 0) {
        return NULL;
    }

    void* pBuffer = pAllocatable->Alloc(fileSize);
    if (pBuffer == NULL) {
        return NULL;
    }

    if (ReadFile(id, pBuffer, fileSize, 0) != fileSize) {
        return NULL;
    }

    DCStoreRange(pBuffer, fileSize);
    return pBuffer;
}

void SoundArchiveLoader::Cancel() {
    if (mStream != NULL && mStream->CanCancel()) {
        if (mStream->CanAsync()) {
            mStream->CancelAsync(NULL, NULL);
        } else {
            mStream->Cancel();
        }
    }
}

} // namespace detail
} // namespace snd
} // namespace nw4r
