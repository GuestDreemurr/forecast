#ifndef NW4R_SND_WSD_PLAYER_H
#define NW4R_SND_WSD_PLAYER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_BasicPlayer.h>
#include <nw4r/snd/snd_Channel.h>
#include <nw4r/snd/snd_DisposeCallback.h>
#include <nw4r/snd/snd_Lfo.h>
#include <nw4r/snd/snd_SoundThread.h>
#include <nw4r/snd/snd_WsdFile.h>

#include <nw4r/ut.h>

namespace nw4r {
namespace snd {
namespace detail {

// Forward declarations
struct WaveData;

class WsdPlayer;

/******************************************************************************
 *
 * WsdCallback
 *
 ******************************************************************************/
class WsdCallback {
public:
    virtual ~WsdCallback() {} // at 0x8

    virtual bool GetWaveSoundData(WaveSoundInfo* pSoundInfo,
                                  WaveSoundNoteInfo* pNoteInfo,
                                  WaveData* pWaveData, const void* pWsdData,
                                  int index, int noteIndex,
                                  u32 callbackArg) const = 0; // at 0xC
};

/******************************************************************************
 *
 * WsdTrack
 *
 ******************************************************************************/
class WsdTrack {
public:
    WsdTrack() : mPlayer(NULL) {}

    void Init(WsdPlayer* pPlayer);
    void SetData(const void* pWsdData, int index);
    void Close();

    int ParseNextTick(const WsdCallback* pCallback, u32 callbackArg,
                      bool doNoteOn);

    void ReleaseAllChannel(int release);
    void PauseAllChannel(bool flag);
    void UpdateChannel();

    const void* GetWsdDataAddress() const {
        return mWsdData;
    }

private:
    static const int DEFAULT_PRIORITY = 64;
    static const int DEFAULT_BENDRANGE = 2;

private:
    int Parse(const WsdCallback* pCallback, u32 callbackArg, bool doNoteOn);

    void FreeAllChannel() {
        for (Channel* pIt = mChannelList; pIt != NULL;
             pIt = pIt->GetNextTrackChannel()) {

            Channel::FreeChannel(pIt);
        }

        mChannelList = NULL;
    }

    static void ChannelCallbackFunc(Channel* pDropChannel,
                                    Channel::ChannelCallbackStatus status,
                                    u32 callbackArg);

private:
    const void* mWsdData; // at 0x0
    int mWsdIndex;        // at 0x4
    int mNoteCount;       // at 0x8

    LfoParam mLfoParam; // at 0xC
    u8 mBendRange;      // at 0x1C
    u8 mPriority;       // at 0x1D

    WaveSoundInfo mWaveSoundInfo; // at 0x20
    WsdPlayer* mPlayer;           // at 0x2C
    Channel* mChannelList;        // at 0x30
};

/******************************************************************************
 *
 * WsdPlayer
 *
 ******************************************************************************/
class WsdPlayer : public BasicPlayer, public DisposeCallback {
public:
    typedef detail::WsdCallback WsdCallback;

public:
    WsdPlayer();

    virtual bool Start();          // at 0xC
    virtual void Stop();           // at 0x10
    virtual void Pause(bool flag); // at 0x14

    virtual bool IsActive() const {
        return mActiveFlag;
    } // at 0x18

    virtual bool IsStarted() const {
        return mStartedFlag;
    } // at 0x20

    virtual bool IsPrepared() const {
        return mPreparedFlag;
    } // at 0x1C

    virtual bool IsPause() const {
        return mPauseFlag;
    } // at 0x24

    virtual void InvalidateData(const void* pStart,
                                const void* pEnd); // at 0x50

    virtual void InvalidateWaveData(const void* /* pStart */,
                                    const void* /* pEnd */) {} // at 0x54

    bool Prepare(const void* pWsdData, int index, int voices,
                 const WsdCallback* pCallback, u32 callbackArg);

    void SetChannelPriority(int priority);

    f32 GetPanRange() const {
        return mPanRange;
    }

    int GetVoiceOutCount() const {
        return mVoiceOutCount;
    }

    int GetChannelPriority() const {
        return mPriority;
    }

    static void UpdateAllPlayers();
    static void StopAllPlayers();

public:
    NW4R_UT_LINKLIST_NODE_DECL(); // at 0x7C

private:
    static const int DEFAULT_PRIORITY = 64;

private:
    inline void InitParam(int voices, const WsdCallback* pCallback,
                          u32 callbackArg);

    inline void FinishPlayer();
    inline void Update();
    inline bool ParseNextTick(bool doNoteOn);

private:
    u8 mActiveFlag;   // at 0x84
    u8 mPreparedFlag; // at 0x85
    u8 mStartedFlag;  // at 0x86
    u8 mPauseFlag;    // at 0x87
    u8 mSkipFlag;     // at 0x88

    f32 mPanRange;      // at 0x8C
    int mVoiceOutCount; // at 0x90
    u8 mPriority;       // at 0x94

    const WsdCallback* mCallback; // at 0x98
    u32 mCallbackData;            // at 0x9C

    WsdTrack mTrack;  // at 0xA0
    u32 mTickCounter; // at 0xD4
};

NW4R_UT_LINKLIST_TYPEDEF_DECL(WsdPlayer);

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
