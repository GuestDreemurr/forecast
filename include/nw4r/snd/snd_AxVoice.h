#ifndef NW4R_SND_AX_VOICE_H
#define NW4R_SND_AX_VOICE_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_Types.h>

#include <nw4r/ut.h>

#include <revolution/AX.h>

#include <climits>

namespace nw4r {
namespace snd {
namespace detail {

inline int CalcAxvpbDelta(u16 init, u16 target) {
    return (target - init) / AX_SAMPLES_PER_FRAME;
}

inline u16 CalcMixVolume(f32 volume) {
    return ut::Min<u32>(USHRT_MAX, AX_MAX_VOLUME * volume);
}

class AxVoice {
    friend class AxVoiceManager;

public:
    enum AxVoiceCallbackStatus {
        CALLBACK_STATUS_CANCEL,
        CALLBACK_STATUS_DROP_DSP
    };

    typedef void (*AxVoiceCallback)(AxVoice* pDropVoice,
                                    AxVoiceCallbackStatus status, void* pArg);

    enum Format {
        FORMAT_ADPCM = 0,
        FORMAT_PCM16 = 10,
        FORMAT_PCM8 = 25,
    };

    enum VoiceType {
        VOICE_TYPE_NORMAL,
        VOICE_TYPE_STREAM,
    };

    enum SrcType {
        SRC_NONE,
        SRC_LINEAR,
        SRC_4TAP_8K,
        SRC_4TAP_12K,
        SRC_4TAP_16K,
        SRC_4TAP_AUTO,
    };

    struct MixParam {
        u16 vL;     // at 0x0
        u16 vR;     // at 0x2
        u16 vS;     // at 0x4
        u16 vAuxAL; // at 0x6
        u16 vAuxAR; // at 0x8
        u16 vAuxAS; // at 0xA
        u16 vAuxBL; // at 0xC
        u16 vAuxBR; // at 0xE
        u16 vAuxBS; // at 0x10
        u16 vAuxCL; // at 0x12
        u16 vAuxCR; // at 0x14
        u16 vAuxCS; // at 0x16
    };

    struct RemoteMixParam {
        u16 vMain0; // at 0x0
        u16 vAux0;  // at 0x2
        u16 vMain1; // at 0x4
        u16 vAux1;  // at 0x6
        u16 vMain2; // at 0x8
        u16 vAux2;  // at 0xA
        u16 vMain3; // at 0xC
        u16 vAux3;  // at 0xE
    };

public:
    AxVoice();

    bool IsRun() const {
        ut::AutoInterruptLock lock;

        if (mVpb == NULL) {
            return false;
        }

        return mVpb->pb.state == AX_VOICE_RUN;
    }

    Format GetFormat() const {
        return mFormat;
    }

    bool IsAvailable() const {
        return mVpb != NULL;
    }

    void SetBaseAddress(const void* pBase) {
        mWaveData = pBase;
    }

    f32 GetDspRatio(f32 ratio) const {
        ut::AutoInterruptLock lock;
        return (ratio * mSampleRate) / AX_SAMPLE_RATE;
    }

    void Setup(const void* pWave, Format fmt, int rate);

    bool IsPlayFinished() const;
    void SetLoopStart(const void* pBase, u32 samples);
    void SetLoopEnd(const void* pBase, u32 samples);
    void SetLoopFlag(bool loop);
    void StopAtPoint(const void* pBase, u32 samples);
    bool IsCurrentAddressCoverd(const void* pBegin, const void* pEnd) const;
    bool IsDataAddressCoverd(const void* pBegin, const void* pEnd) const;

    u32 GetCurrentPlayingSample() const;

    u32 GetCurrentPlayingDspAddress() const {
        ut::AutoInterruptLock lock;

        const AXPB* pPb = &mVpb->pb;
        if (pPb == NULL) {
            return 0;
        }

        return (pPb->addr.currentAddressHi << 16) + pPb->addr.currentAddressLo;
    }

    u32 GetLoopEndDspAddress() const {
        ut::AutoInterruptLock lock;

        const AXPB* pPb = &mVpb->pb;
        if (pPb == NULL) {
            return 0;
        }

        return (pPb->addr.endAddressHi << 16) + pPb->addr.endAddressLo;
    }

    void Run();
    void Stop();

    void SetPriority(u32 priority);
    void SetVoiceType(VoiceType type);
    void SetRmtMix(const AXPBRMTMIX& rMix);
    void EnableRemote();
    void DisableRemote();

    void SetAddr(bool loop, const void* pWave, u32 loopStart, u32 loopEnd);
    void SetSrcType(SrcType type, f32 pitch);
    void SetAdpcm(const AdpcmParam* pParam);
    void SetAdpcmLoop(const AdpcmLoopParam* pParam);
    bool SetMix(const MixParam& rParam);
    void SetSrc(f32 ratio, bool initial);
    bool SetVe(f32 volume, f32 initVolume);
    void SetLpf(u16 freq);

    static u32 GetDspAddressBySample(const void* pBase, u32 samples,
                                     Format fmt);
    static u32 GetSampleByDspAddress(const void* pBase, u32 addr, Format fmt);
    static u32 GetSampleByByte(u32 addr, Format fmt);

private:
    static void VoiceCallback(void* pArg);

private:
    AXVPB* mVpb;               // at 0x0
    const void* mWaveData;     // at 0x4
    Format mFormat;            // at 0x8
    int mSampleRate;           // at 0xC
    bool mActiveFlag;          // at 0x10
    bool mFirstVeUpdateFlag;   // at 0x11
    bool mFirstMixUpdateFlag;  // at 0x12
    u16 mVolumePrev;           // at 0x14
    MixParam mMixPrev;         // at 0x16
    AxVoiceCallback mCallback; // at 0x30
    void* mCallbackData;       // at 0x34
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
