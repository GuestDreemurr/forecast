#ifndef NW4R_SND_WAVE_PLAYER_H
#define NW4R_SND_WAVE_PLAYER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_WaveFile.h>
#include <nw4r/snd/snd_Voice.h>

#include <nw4r/ut.h>

#include <revolution/WPAD.h>

namespace nw4r {
namespace snd {
namespace detail {

/**
 * Player for wave buffers which are queued by the user.
 * (Not instantiated in this binary: only the code reachable from the sound
 * thread was linked, so names here are unofficial.)
 */
class WavePlayer {
public:
    enum SampleFormat {
        SAMPLE_FORMAT_NONE,
        SAMPLE_FORMAT_PCM16,
        SAMPLE_FORMAT_PCM8
    };

    enum CallbackStatus {
        CALLBACK_STATUS_FINISH,
        CALLBACK_STATUS_CANCEL
    };

    struct WaveBuffer {
        UNKWORD unk0;
        UNKWORD unk4;
        void* bufferAddress[CHANNEL_MAX]; // at 0x8
        u32 bufferSize;                   // at 0x10
        bool busyFlag;                    // at 0x14

        NW4R_UT_LINKLIST_NODE_DECL(); // at 0x18
    };

    NW4R_UT_LINKLIST_TYPEDEF_DECL(WaveBuffer);

    typedef void (*Callback)(CallbackStatus status, WavePlayer* pPlayer,
                             WaveBuffer* pBuffer, void* pCallbackArg);

public:
    void Stop();

    static void UpdateBufferAllPlayers();
    static void UpdateAllPlayers();
    static void StopAllPlayers();

private:
    void StartVoice();
    void SwitchBuffer();
    void Update();

    inline void UpdateBuffer();

    void StopVoice() {
        ut::AutoInterruptLock lock;

        if (mVoice != NULL) {
            mVoice->Stop();
        }

        mStartedFlag = false;
    }

    bool IsBufferEnd() const {
        if (mVoice == NULL) {
            return false;
        }

        if (mVoice->IsPlayFinished()) {
            return false;
        }

        if (mWaveBufferList.IsEmpty()) {
            return false;
        }

        const WaveBuffer& rBuffer = mWaveBufferList.GetFront();

        return !mVoice->IsCurrentAddressCoverd(
            0, rBuffer.bufferAddress[0],
            static_cast<u8*>(rBuffer.bufferAddress[0]) + rBuffer.bufferSize);
    }

private:
    UNKWORD unk0;
    WaveBufferList mWaveBufferList; // at 0x4
    int mChannelCount;              // at 0x10
    UNKWORD unk14;
    Voice* mVoice; // at 0x18

    bool mActiveFlag;  // at 0x1C
    bool mStartedFlag; // at 0x1D
    bool mLoopSetFlag; // at 0x1E
    bool mPauseFlag;   // at 0x1F

    SampleFormat mSampleFormat; // at 0x20
    int mSampleRate;            // at 0x24
    s64 mPlaySampleCount;       // at 0x28

    f32 mVolume;                                // at 0x30
    f32 mPan;                                   // at 0x34
    f32 mSurroundPan;                           // at 0x38
    f32 mPitch;                                 // at 0x3C
    f32 mLpfFreq;                               // at 0x40
    int mOutputLine;                            // at 0x44
    f32 mMainOutVolume;                         // at 0x48
    f32 mRemoteOutVolume[WPAD_MAX_CONTROLLERS]; // at 0x4C
    f32 mMainSend;                              // at 0x5C
    f32 mFxSend[AUX_BUS_NUM];                   // at 0x60
    f32 mRemoteSend[WPAD_MAX_CONTROLLERS];      // at 0x6C
    f32 mRemoteFxSend[WPAD_MAX_CONTROLLERS];    // at 0x7C

    Callback mCallback;  // at 0x8C
    void* mCallbackArg;  // at 0x90

public:
    NW4R_UT_LINKLIST_NODE_DECL(); // at 0x94
};

NW4R_UT_LINKLIST_TYPEDEF_DECL(WavePlayer);

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
