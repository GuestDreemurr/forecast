#ifndef NW4R_SND_VOICE_H
#define NW4R_SND_VOICE_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_AxVoice.h>
#include <nw4r/snd/snd_DisposeCallback.h>
#include <nw4r/snd/snd_Types.h>

#include <nw4r/ut.h>

#include <revolution/WPAD.h>

namespace nw4r {
namespace snd {
namespace detail {

// Forward declarations
class WaveData;

class Voice : public DisposeCallback {
    friend class VoiceManager;

public:
    enum VoiceCallbackStatus {
        CALLBACK_STATUS_FINISH_WAVE,
        CALLBACK_STATUS_CANCEL,
        CALLBACK_STATUS_DROP_VOICE,
        CALLBACK_STATUS_DROP_DSP,
    };

    typedef void (*VoiceCallback)(Voice* pDropVoice, VoiceCallbackStatus status,
                                  void* pCallbackArg);

    enum VoiceSyncFlag {
        SYNC_AX_SRC_INITIAL = (1 << 0),
        SYNC_AX_VOICE = (1 << 1),
        SYNC_AX_SRC = (1 << 3),
        SYNC_AX_VE = (1 << 4),
        SYNC_AX_MIX = (1 << 5),
        SYNC_AX_LPF = (1 << 6),
        SYNC_AX_REMOTE = (1 << 7),
        SYNC_AX_BIQUAD = (1 << 8),
    };

    static const int PRIORITY_MAX = 255;

public:
    Voice();
    virtual ~Voice(); // at 0x8

    virtual void InvalidateData(const void* /* pStart */,
                                const void* /* pEnd */) {} // at 0xC

    virtual void InvalidateWaveData(const void* pStart,
                                    const void* pEnd); // at 0x10

    int GetPriority() const {
        return mPriority;
    }

    int GetAxVoiceCount() const {
        return mChannelCount * mVoiceOutCount;
    }

    bool IsActive() const {
        ut::AutoInterruptLock lock;
        return mAxVoice[0][0] != NULL;
    }

    void InitParam(int channels, int voices, VoiceCallback pCallback,
                   void* pCallbackArg);
    void Update();

    bool Acquire(int channels, int voices, int priority,
                 VoiceCallback pCallback, void* pCallbackArg);
    void Free();
    void Setup(const WaveData& rData);

    void Start();
    void Stop();
    void Pause(bool flag);

    AxVoice::Format GetFormat() const;

    void SetVolume(f32 volume);
    void SetVeVolume(f32 target, f32 init);
    void SetPitch(f32 pitch);

    void SetPan(f32 pan);
    void SetSurroundPan(f32 pan);
    void SetAmbientPan(f32 pan);
    void SetAmbientSurroundPan(f32 pan);

    void SetLpfFreq(f32 freq);
    void SetOutputLine(int flag);

    void SetMainOutVolume(f32 volume);
    void SetMainSend(f32 send);
    void SetFxSend(AuxBus bus, f32 send);

    void SetRemoteOutVolume(int remote, f32 volume);
    void SetRemoteSend(int remote, f32 send);
    void SetRemoteFxSend(int remote, f32 send);

    void SetPriority(int priority);
    void UpdateVoicesPriority();

    bool IsCurrentAddressCoverd(int channel, const void* pBegin,
                                const void* pEnd) const;

    void SetAdpcmLoop(int channel, const AdpcmLoopParam* pParam);
    void SetBaseAddress(int channel, const void* pBase);

    bool IsPlayFinished() const;
    u32 GetCurrentPlayingSample() const;

    void SetLoopStart(int channel, const void* pBase, u32 samples);
    void SetLoopEnd(int channel, const void* pBase, u32 samples);
    void SetLoopFlag(bool loop);
    void StopAtPoint(int channel, const void* pBase, u32 samples);
    void SetVoiceType(AxVoice::VoiceType type);

    void CalcAxSrc(bool initial);
    bool CalcAxVe();
    bool CalcAxMix();
    void CalcAxLpf();

private:
    static const int VOICES_MIN = 1;
    static const int VOICES_MAX = 4;

private:
    static void AxVoiceCallbackFunc(AxVoice* pVoice,
                                    AxVoice::AxVoiceCallbackStatus status,
                                    void* pCallbackArg);

    void TransformDpl2Pan(f32* pPan, f32* pSurroundPan, f32 pan,
                          f32 surroundPan);
    void CalcMixParam(int channel, int voice, AxVoice::MixParam* pMix);
    void CalcRemoteMixParam(int channel, int voice, AXPBRMTMIX* pMix);

    void RunAllAxVoice() {
        ut::AutoInterruptLock lock;

        for (int i = 0; i < mChannelCount; i++) {
            for (int j = 0; j < mVoiceOutCount; j++) {
                if (mAxVoice[i][j] != NULL) {
                    mAxVoice[i][j]->Run();
                }
            }
        }
    }

    void StopAllAxVoice() {
        ut::AutoInterruptLock lock;

        for (int i = 0; i < mChannelCount; i++) {
            for (int j = 0; j < mVoiceOutCount; j++) {
                if (mAxVoice[i][j] != NULL) {
                    mAxVoice[i][j]->Stop();
                }
            }
        }
    }

private:
    AxVoice* mAxVoice[CHANNEL_MAX][VOICES_MAX]; // at 0xC
    SoundParam mVoiceOutParam[VOICES_MAX];      // at 0x2C
    int mChannelCount;                          // at 0x9C
    int mVoiceOutCount;                         // at 0xA0

    VoiceCallback mCallback; // at 0xA4
    void* mCallbackArg;      // at 0xA8

    bool mIsActive;   // at 0xAC
    bool mIsStarting; // at 0xAD
    bool mIsStarted;  // at 0xAE
    bool mIsPause;    // at 0xAF
    bool mIsPausing;  // at 0xB0

    u8 mSyncFlag;                               // at 0xB1
    int mPriority;                              // at 0xB4
    f32 mPan;                                   // at 0xB8
    f32 mSurroundPan;                           // at 0xBC
    f32 mAmbientPan;                            // at 0xC0
    f32 mAmbientSurroundPan;                    // at 0xC4
    f32 mLpfFreq;                               // at 0xC8
    int mOutputLineFlag;                        // at 0xCC
    f32 mMainOutVolume;                         // at 0xD0
    f32 mMainSend;                              // at 0xD4
    f32 mFxSend[AUX_BUS_NUM];                   // at 0xD8
    f32 mRemoteOutVolume[WPAD_MAX_CONTROLLERS]; // at 0xE4
    f32 mRemoteSend[WPAD_MAX_CONTROLLERS];      // at 0xF4
    f32 mRemoteFxSend[WPAD_MAX_CONTROLLERS];    // at 0x104
    f32 mPitch;                                 // at 0x114
    f32 mVolume;                                // at 0x118
    f32 mVeInitVolume;                          // at 0x11C
    f32 mVeTargetVolume;                        // at 0x120

public:
    NW4R_UT_LINKLIST_NODE_DECL(); // at 0x124
};

NW4R_UT_LINKLIST_TYPEDEF_DECL(Voice);

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
