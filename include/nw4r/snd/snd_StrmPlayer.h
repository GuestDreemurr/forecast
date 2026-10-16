#ifndef NW4R_SND_STRM_PLAYER_H
#define NW4R_SND_STRM_PLAYER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_BasicPlayer.h>
#include <nw4r/snd/snd_StrmChannel.h>
#include <nw4r/snd/snd_StrmFile.h>
#include <nw4r/snd/snd_Voice.h>

#include <nw4r/ut.h>

#include <revolution/OS.h>

namespace nw4r {
namespace snd {
namespace detail {

class StrmPlayer : public BasicPlayer {
public:
    enum StartOffsetType {
        START_OFFSET_TYPE_SAMPLE,
        START_OFFSET_TYPE_MILLISEC
    };

    enum LoadResult {
        LOAD_RESULT_FINISHED, // Loaded immediately
        LOAD_RESULT_FAILED,   // Could not start the load
        LOAD_RESULT_CANCELED,
        LOAD_RESULT_ASYNC // Callback will be notified later
    };

    struct StrmHeader {
        static const int STRM_CHANNEL_MAX = 8;

        StrmInfo strmInfo;                     // at 0x0
        AdpcmInfo adpcmInfo[STRM_CHANNEL_MAX]; // at 0x38
    };

    /******************************************************************************
     * StrmDataLoadCallback
     ******************************************************************************/
    class StrmDataLoadCallback : private ut::NonCopyable {
        friend class StrmPlayer;

    public:
        StrmDataLoadCallback() {}

        virtual void OnLoad(bool success); // at 0x8

        void SetAdpcmLoopContext(int channels, u16* pPredScale);
        void* GetBuffer(int channel);

    private:
        StrmPlayer* mStrmPlayer; // at 0x4
        BOOL mStartedFlag;       // at 0x8
        s32 mDataBlockIndex;     // at 0xC
        s32 mBufferBlockIndex;   // at 0x10

    public:
        NW4R_UT_LINKLIST_NODE_DECL(); // at 0x14
    };

    NW4R_UT_LINKLIST_TYPEDEF_DECL(StrmDataLoadCallback);

    /******************************************************************************
     * StrmLoader
     ******************************************************************************/
    class StrmLoader {
    public:
        typedef void (*HeaderCallback)(bool success, const StrmHeader* pHeader,
                                       void* pCallbackArg);

    public:
        virtual ~StrmLoader() {} // at 0x8

        virtual LoadResult LoadHeader(HeaderCallback pCallback,
                                      void* pCallbackArg, StrmPlayer* pPlayer,
                                      u32 userData) = 0; // at 0xC

        virtual LoadResult LoadData(void* pBuffer, u32 size, s32 offset,
                                    int channels, u32 blockSize,
                                    u32 blockHeaderOffset,
                                    bool needUpdateAdpcmLoop,
                                    StrmDataLoadCallback* pCallback,
                                    StrmPlayer* pPlayer,
                                    u32 userData) = 0; // at 0x10

        virtual void Cancel(StrmPlayer* pPlayer, u32 userData) = 0; // at 0x14
    };

public:
    StrmPlayer();

    virtual bool Start();          // at 0xC
    virtual void Stop();           // at 0x10
    virtual void Pause(bool flag); // at 0x14

    virtual bool IsActive() const {
        return mActiveFlag;
    } // at 0x18
    virtual bool IsPrepared() const {
        return mPreparedFlag;
    } // at 0x1C
    virtual bool IsStarted() const {
        return mStartedFlag;
    } // at 0x20
    virtual bool IsPause() const {
        return mPauseFlag;
    } // at 0x24

    bool Prepare(StrmBufferPool* pBufferPool, StartOffsetType offsetType,
                 int offset, int voices, StrmLoader* pLoader, u32 userData);

    void InitParam(int voices);

    void Update();
    void UpdateBuffer();

    static void UpdateBufferAllPlayers();
    static void UpdateAllPlayers();
    static void StopAllPlayers();

public:
    NW4R_UT_LINKLIST_NODE_DECL(); // at 0x70

private:
    static const int DATA_BLOCK_COUNT_MIN = 4;
    static const int DATA_BLOCK_COUNT_MAX = 32;
    static const int DATA_BLOCK_SIZE_MAX = 0x2000;

    static const int LOAD_BUFFER_SIZE = 0x4000 + 32;

private:
    void Shutdown();

    void OnHeaderLoad(const StrmHeader* pStrmHeader);
    bool SetupPlayer(const StrmHeader* pStrmHeader);

    bool AllocChannels(int channels, int voices);
    void FreeChannels();

    void UpdatePlayingBlockIndex();
    void UpdateLoadingBlockIndex(bool started);

    inline void UpdateLoopAddress(u32 startSample, u32 endSample);
    inline void SetLoopEndToZeroBuffer(int endBlock);
    inline void UpdatePauseStatus();

    inline int CalcLoadingBufferBlockCount() const;

    inline StrmDataLoadCallback* AllocLoadCallback();

    static void HeaderCallbackFunc(bool success, const StrmHeader* pHeader,
                                   void* pCallbackArg);

    static void VoiceCallbackFunc(Voice* pDropVoice,
                                  Voice::VoiceCallbackStatus status,
                                  void* pCallbackArg);

private:
    StrmInfo mStrmInfo; // at 0x78

    u8 mActiveFlag;          // at 0xB0
    u8 mStartedFlag;         // at 0xB1
    u8 mPreparedFlag;        // at 0xB2
    u8 mPauseFlag;           // at 0xB3
    u8 mDiskErrorFlag;       // at 0xB4
    u8 mPauseStatus;         // at 0xB5
    u8 mLoadWaitFlag;        // at 0xB6
    u8 mNoRealtimeLoadFlag;  // at 0xB7
    u8 mSkipUpdateAdpcmLoop; // at 0xB8
    u8 mValidAdpcmLoop;      // at 0xB9
    u8 mPlayFinishFlag;      // at 0xBA
    u8 mLoadFinishFlag;      // at 0xBB

    s32 mLoopCounter;    // at 0xBC
    int mPrepareCounter; // at 0xC0

    int mChangeNumBlocks;      // at 0xC4
    int mDataBlockSize;        // at 0xC8
    int mBufferBlockCount;     // at 0xCC
    int mBufferBlockCountBase; // at 0xD0

    int mLoadingBufferBlockCount; // at 0xD4
    int mLoadingBufferBlockIndex; // at 0xD8
    int mLoadingDataBlockIndex;   // at 0xDC

    int mPlayingBufferBlockCount; // at 0xE0
    int mPlayingBufferBlockIndex; // at 0xE4
    int mPlayingDataBlockIndex;   // at 0xE8
    int mLoopStartBlockIndex;     // at 0xEC
    int mLastBlockIndex;          // at 0xF0

    StartOffsetType mStartOffsetType; // at 0xF4
    int mStartOffset;                 // at 0xF8

    StrmDataLoadCallbackList mFreeLoadCallbackList;               // at 0xFC
    StrmDataLoadCallbackList mLoadCallbackList;                   // at 0x108
    StrmDataLoadCallback mLoadCallbackArea[DATA_BLOCK_COUNT_MAX]; // at 0x114

    StrmBufferPool* mBufferPool; // at 0x494
    StrmLoader* mLoader;         // at 0x498
    u32 mLoaderUserData;         // at 0x49C
    Voice* mVoice;               // at 0x4A0
    s32 mChannelCount;           // at 0x4A4
    s32 mVoiceOutCount;          // at 0x4A8

    StrmChannel mChannels[CHANNEL_MAX];   // at 0x4AC
    u16 mAdpcmLoopPredScale[CHANNEL_MAX]; // at 0x51C

    static u8 sLoadBuffer[LOAD_BUFFER_SIZE] ALIGN(32);
};

NW4R_UT_LINKLIST_TYPEDEF_DECL(StrmPlayer);

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
