#include <nw4r/snd.h>
#include <nw4r/ut.h>

#include <cstring>

namespace nw4r {
namespace snd {
namespace detail {

namespace {

inline void SetVoiceLoopAddr(AXVPB* pVpb, u32 addr) {
    ut::AutoInterruptLock lock;

    pVpb->pb.addr.loopAddressHi = addr >> 16;
    pVpb->pb.addr.loopAddressLo = addr;

    if (!(pVpb->sync & AX_PBSYNC_ADDR)) {
        pVpb->sync |= AX_PBSYNC_LOOP_ADDR;
    }
}

inline void SetVoiceEndAddr(AXVPB* pVpb, u32 addr) {
    ut::AutoInterruptLock lock;

    pVpb->pb.addr.endAddressHi = addr >> 16;
    pVpb->pb.addr.endAddressLo = addr;

    if (!(pVpb->sync & AX_PBSYNC_ADDR)) {
        pVpb->sync |= AX_PBSYNC_END_ADDR;
    }
}

inline void SetVoiceLoop(AXVPB* pVpb, u16 loop) {
    ut::AutoInterruptLock lock;

    pVpb->pb.addr.loopFlag = loop;

    if (!(pVpb->sync & AX_PBSYNC_ADDR)) {
        pVpb->sync |= AX_PBSYNC_LOOP_FLAG;
    }
}

} // namespace

AxVoice::AxVoice()
    : mWaveData(NULL),
      mActiveFlag(false),
      mFirstVeUpdateFlag(false),
      mFirstMixUpdateFlag(false) {}

void AxVoice::Setup(const void* pWave, Format fmt, int rate) {
    ut::AutoInterruptLock lock;

    mWaveData = pWave;
    mFormat = fmt;
    mSampleRate = rate;

    std::memset(&mMixPrev, 0, sizeof(mMixPrev));

    mFirstVeUpdateFlag = true;
    mFirstMixUpdateFlag = true;
}

bool AxVoice::IsPlayFinished() const {
    ut::AutoInterruptLock lock;

    if (mWaveData == NULL) {
        return false;
    }

    u32 dspAddr = GetCurrentPlayingDspAddress();
    const void* pBuffer = AxManager::GetInstance().GetZeroBufferAddress();
    u32 samples = GetDspAddressBySample(pBuffer, 0, mFormat);

    u32 addr = samples;

    switch (mFormat) {
    case FORMAT_ADPCM: {
        addr += 0x200;
        break;
    }

    case FORMAT_PCM8: {
        addr += 0x100;
        break;
    }

    case FORMAT_PCM16: {
        addr += 0x80;
        break;
    }
    }

    if (samples <= dspAddr && dspAddr < addr) {
        return true;
    }

    return false;
}

void AxVoice::SetLoopStart(const void* pBase, u32 samples) {
    ut::AutoInterruptLock lock;

    u32 addr = GetDspAddressBySample(pBase, samples, mFormat);
    SetVoiceLoopAddr(mVpb, addr);
}

void AxVoice::SetLoopEnd(const void* pBase, u32 samples) {
    ut::AutoInterruptLock lock;

    u32 addr = GetDspAddressBySample(pBase, samples - 1, mFormat);
    SetVoiceEndAddr(mVpb, addr);
}

void AxVoice::SetLoopFlag(bool loop) {
    ut::AutoInterruptLock lock;
    SetVoiceLoop(mVpb, loop);
}

void AxVoice::StopAtPoint(const void* pBase, u32 samples) {
    ut::AutoInterruptLock lock;

    const void* pBuffer = AxManager::GetInstance().GetZeroBufferAddress();
    u32 begin = GetDspAddressBySample(pBuffer, 0, mFormat);
    u32 end = GetDspAddressBySample(pBase, samples - 1, mFormat);

    if (mVpb != NULL) {
        SetVoiceLoopAddr(mVpb, begin);
        SetVoiceEndAddr(mVpb, end);
        SetVoiceLoop(mVpb, false);
    }
}

bool AxVoice::IsCurrentAddressCoverd(const void* pBegin,
                                     const void* pEnd) const {
    ut::AutoInterruptLock lock;

    u32 dspAddr = GetCurrentPlayingDspAddress();

    u32 samples = GetSampleByByte(
        static_cast<const u8*>(pEnd) - static_cast<const u8*>(pBegin), mFormat);

    u32 begin = GetDspAddressBySample(pBegin, 0, mFormat);
    u32 end = GetDspAddressBySample(pBegin, samples, mFormat);

    if (begin <= dspAddr && dspAddr < end) {
        return true;
    }

    return false;
}

bool AxVoice::IsDataAddressCoverd(const void* pBegin, const void* pEnd) const {
    if (mWaveData == NULL) {
        return false;
    }

    return pBegin <= mWaveData && mWaveData <= pEnd;
}

u32 AxVoice::GetCurrentPlayingSample() const {
    ut::AutoInterruptLock lock;

    if (mWaveData == NULL) {
        return 0;
    }

    if (IsPlayFinished()) {
        u32 end = GetLoopEndDspAddress();
        u32 samples = GetSampleByDspAddress(mWaveData, end, mFormat);
        return samples + 1;
    }

    u32 now = GetCurrentPlayingDspAddress();
    u32 samples = GetSampleByDspAddress(mWaveData, now, mFormat);
    return samples;
}

void AxVoice::VoiceCallback(void* pArg) {
    ut::AutoInterruptLock lock;

    AXVPB* pVpb = static_cast<AXVPB*>(pArg);
    AxVoice* pVoice = AxVoiceManager::GetInstance().GetAxVoice(pVpb->index);

    pVoice->mVpb = NULL;

    if (pVoice->mCallback != NULL) {
        pVoice->mCallback(pVoice, CALLBACK_STATUS_DROP_DSP,
                          pVoice->mCallbackData);
    }

    AxVoiceManager::GetInstance().FreeAxVoice(pVoice);
}

u32 AxVoice::GetDspAddressBySample(const void* pBase, u32 samples, Format fmt) {
    if (pBase != NULL) {
        pBase = OSCachedToPhysical(pBase);
    }

    u32 addr = 0;

    switch (fmt) {
    case FORMAT_ADPCM: {
        // clang-format off
        addr = (samples / AX_ADPCM_SAMPLES_PER_FRAME * AX_ADPCM_NIBBLES_PER_FRAME) +
               (samples % AX_ADPCM_SAMPLES_PER_FRAME) +
               (reinterpret_cast<u32>(pBase) * sizeof(u16)) +
               sizeof(u16);
        // clang-format on
        break;
    }

    case FORMAT_PCM8: {
        addr = reinterpret_cast<u32>(pBase) + samples;

        break;
    }
    case FORMAT_PCM16: {
        addr = reinterpret_cast<u32>(pBase) / sizeof(u16) + samples;
        break;
    }
    }

    return addr;
}

u32 AxVoice::GetSampleByDspAddress(const void* pBase, u32 addr, Format fmt) {
    if (pBase != NULL) {
        pBase = OSCachedToPhysical(pBase);
    }

    u32 samples = 0;

    switch (fmt) {
    case FORMAT_ADPCM: {
        samples = addr - reinterpret_cast<u32>(pBase) * sizeof(u16);
        // clang-format off
        samples = (samples % AX_ADPCM_NIBBLES_PER_FRAME) +
                  (samples / AX_ADPCM_NIBBLES_PER_FRAME * AX_ADPCM_SAMPLES_PER_FRAME) -
                  sizeof(u16);
        // clang-format on
        break;
    }

    case FORMAT_PCM8: {
        samples = addr - reinterpret_cast<u32>(pBase);
        break;
    }

    case FORMAT_PCM16: {
        samples = addr - reinterpret_cast<u32>(pBase) / sizeof(u16);
        break;
    }
    }

    return samples;
}

u32 AxVoice::GetSampleByByte(u32 addr, Format fmt) {
    u32 samples = 0;
    u32 frac;

    switch (fmt) {
    case FORMAT_ADPCM: {
        samples = addr / AX_ADPCM_FRAME_SIZE * AX_ADPCM_SAMPLES_PER_FRAME;
        frac = addr % AX_ADPCM_FRAME_SIZE;
        if (frac != 0) {
            samples += (frac - 1) * sizeof(u16);
        }
        break;
    }

    case FORMAT_PCM8: {
        samples = addr;
        break;
    }

    case FORMAT_PCM16: {
        samples = addr / sizeof(u16);
        break;
    }
    }

    return samples;
}

void AxVoice::Run() {
    ut::AutoInterruptLock lock;
    AXSetVoiceState(mVpb, AX_VOICE_RUN);
}

void AxVoice::Stop() {
    ut::AutoInterruptLock lock;

    if (IsRun()) {
        AXSetVoiceState(mVpb, AX_VOICE_STOP);
    }
}

void AxVoice::SetPriority(u32 priority) {
    ut::AutoInterruptLock lock;
    AXSetVoicePriority(mVpb, priority);
}

void AxVoice::SetVoiceType(VoiceType type) {
    ut::AutoInterruptLock lock;
    AXSetVoiceType(mVpb, type);
}

void AxVoice::SetRmtMix(const AXPBRMTMIX& rMix) {
    ut::AutoInterruptLock lock;
    AXSetVoiceRmtMix(mVpb, const_cast<AXPBRMTMIX*>(&rMix));
}

void AxVoice::EnableRemote() {
    ut::AutoInterruptLock lock;
    AXSetVoiceRmtOn(mVpb, 1);
}

void AxVoice::DisableRemote() {
    ut::AutoInterruptLock lock;
    AXSetVoiceRmtOn(mVpb, 0);
}

void AxVoice::SetAddr(bool loop, const void* pWave, u32 loopStart,
                      u32 loopEnd) {
    ut::AutoInterruptLock lock;

    u32 loopAddr;
    if (loop) {
        loopAddr = GetDspAddressBySample(pWave, loopStart, mFormat);
    } else {
        const void* pBuffer = AxManager::GetInstance().GetZeroBufferAddress();
        loopAddr = GetDspAddressBySample(pBuffer, 0, mFormat);
    }

    u32 currAddr = GetDspAddressBySample(pWave, 0, mFormat);
    u32 endAddr = GetDspAddressBySample(pWave, loopEnd - 1, mFormat);

    AXPBADDR addr;
    addr.loopFlag = loop;
    addr.format = mFormat;
    addr.loopAddressHi = loopAddr >> 16;
    addr.loopAddressLo = loopAddr;
    addr.endAddressHi = endAddr >> 16;
    addr.endAddressLo = endAddr;
    addr.currentAddressHi = currAddr >> 16;
    addr.currentAddressLo = currAddr;

    AXSetVoiceAddr(mVpb, &addr);
}

void AxVoice::SetSrcType(SrcType type, f32 pitch) {
    ut::AutoInterruptLock lock;

    if (type == SRC_4TAP_AUTO) {
        f32 ratio = GetDspRatio(pitch);

        if (ratio > 4.0f / 3.0f) {
            type = SRC_4TAP_8K;
        } else if (ratio > 1.0f) {
            type = SRC_4TAP_12K;
        } else {
            type = SRC_4TAP_16K;
        }
    }

    AXSetVoiceSrcType(mVpb, type);
}

void AxVoice::SetAdpcm(const AdpcmParam* pParam) {
    ut::AutoInterruptLock lock;
    AXPBADPCM adpcm;

    switch (mFormat) {
    case FORMAT_ADPCM: {
        std::memcpy(adpcm.a, pParam->coef, sizeof(adpcm.a));
        adpcm.gain = pParam->gain;
        adpcm.pred_scale = pParam->pred_scale;
        adpcm.yn1 = pParam->yn1;
        adpcm.yn2 = pParam->yn2;
        break;
    }

    case FORMAT_PCM16: {
        std::memset(adpcm.a, 0, sizeof(adpcm.a));
        adpcm.gain = 0x800;
        adpcm.pred_scale = 0;
        adpcm.yn1 = 0;
        adpcm.yn2 = 0;
        break;
    }

    case FORMAT_PCM8: {
        std::memset(adpcm.a, 0, sizeof(adpcm.a));
        adpcm.gain = 0x100;
        adpcm.pred_scale = 0;
        adpcm.yn1 = 0;
        adpcm.yn2 = 0;
        break;
    }
    }

    AXSetVoiceAdpcm(mVpb, &adpcm);
}

void AxVoice::SetAdpcmLoop(const AdpcmLoopParam* pParam) {
    ut::AutoInterruptLock lock;
    AXPBADPCMLOOP loop;

    if (mFormat == FORMAT_ADPCM) {
        loop.loop_pred_scale = pParam->loop_pred_scale;
        loop.loop_yn1 = pParam->loop_yn1;
        loop.loop_yn2 = pParam->loop_yn2;
    } else {
        loop.loop_pred_scale = 0;
        loop.loop_yn1 = 0;
        loop.loop_yn2 = 0;
    }

    AXSetVoiceAdpcmLoop(mVpb, &loop);
}

bool AxVoice::SetMix(const MixParam& rParam) {
    ut::AutoInterruptLock lock;

    if (mFirstMixUpdateFlag || !IsRun()) {
        mMixPrev = rParam;
        mFirstMixUpdateFlag = false;
    }

    bool needUpdate = false;

    if (mMixPrev.vL != rParam.vL) {
        needUpdate = true;
    }

    if (mMixPrev.vR != rParam.vR) {
        needUpdate = true;
    }

    if (mMixPrev.vS != rParam.vS) {
        needUpdate = true;
    }

    if (mMixPrev.vAuxAL != rParam.vAuxAL) {
        needUpdate = true;
    }

    if (mMixPrev.vAuxAR != rParam.vAuxAR) {
        needUpdate = true;
    }

    if (mMixPrev.vAuxAS != rParam.vAuxAS) {
        needUpdate = true;
    }

    if (mMixPrev.vAuxBL != rParam.vAuxBL) {
        needUpdate = true;
    }

    if (mMixPrev.vAuxBR != rParam.vAuxBR) {
        needUpdate = true;
    }

    if (mMixPrev.vAuxBS != rParam.vAuxBS) {
        needUpdate = true;
    }

    if (mMixPrev.vAuxCL != rParam.vAuxCL) {
        needUpdate = true;
    }

    if (mMixPrev.vAuxCR != rParam.vAuxCR) {
        needUpdate = true;
    }

    if (mMixPrev.vAuxCS != rParam.vAuxCS) {
        needUpdate = true;
    }

    AXPBMIX mix;

    mix.vL = mMixPrev.vL;
    mix.vR = mMixPrev.vR;
    mix.vS = mMixPrev.vS;

    mix.vAuxAL = mMixPrev.vAuxAL;
    mix.vAuxAR = mMixPrev.vAuxAR;
    mix.vAuxAS = mMixPrev.vAuxAS;

    mix.vAuxBL = mMixPrev.vAuxBL;
    mix.vAuxBR = mMixPrev.vAuxBR;
    mix.vAuxBS = mMixPrev.vAuxBS;

    mix.vAuxCL = mMixPrev.vAuxCL;
    mix.vAuxCR = mMixPrev.vAuxCR;
    mix.vAuxCS = mMixPrev.vAuxCS;

    int vDeltaL = CalcAxvpbDelta(mMixPrev.vL, rParam.vL);
    int vDeltaR = CalcAxvpbDelta(mMixPrev.vR, rParam.vR);
    int vDeltaS = CalcAxvpbDelta(mMixPrev.vS, rParam.vS);

    int vDeltaAuxAL = CalcAxvpbDelta(mMixPrev.vAuxAL, rParam.vAuxAL);
    int vDeltaAuxAR = CalcAxvpbDelta(mMixPrev.vAuxAR, rParam.vAuxAR);
    int vDeltaAuxAS = CalcAxvpbDelta(mMixPrev.vAuxAS, rParam.vAuxAS);

    int vDeltaAuxBL = CalcAxvpbDelta(mMixPrev.vAuxBL, rParam.vAuxBL);
    int vDeltaAuxBR = CalcAxvpbDelta(mMixPrev.vAuxBR, rParam.vAuxBR);
    int vDeltaAuxBS = CalcAxvpbDelta(mMixPrev.vAuxBS, rParam.vAuxBS);

    int vDeltaAuxCL = CalcAxvpbDelta(mMixPrev.vAuxCL, rParam.vAuxCL);
    int vDeltaAuxCR = CalcAxvpbDelta(mMixPrev.vAuxCR, rParam.vAuxCR);
    int vDeltaAuxCS = CalcAxvpbDelta(mMixPrev.vAuxCS, rParam.vAuxCS);

    mix.vDeltaL = vDeltaL;
    mix.vDeltaR = vDeltaR;
    mix.vDeltaS = vDeltaS;

    mix.vDeltaAuxAL = vDeltaAuxAL;
    mix.vDeltaAuxAR = vDeltaAuxAR;
    mix.vDeltaAuxAS = vDeltaAuxAS;

    mix.vDeltaAuxBL = vDeltaAuxBL;
    mix.vDeltaAuxBR = vDeltaAuxBR;
    mix.vDeltaAuxBS = vDeltaAuxBS;

    mix.vDeltaAuxCL = vDeltaAuxCL;
    mix.vDeltaAuxCR = vDeltaAuxCR;
    mix.vDeltaAuxCS = vDeltaAuxCS;

    {
        ut::AutoInterruptLock lock2;
        AXSetVoiceMix(mVpb, &mix);
    }

    if (rParam.vL == 0 || vDeltaL == 0) {
        mMixPrev.vL = rParam.vL;
    } else {
        mMixPrev.vL += vDeltaL * AX_SAMPLES_PER_FRAME;
    }

    if (rParam.vR == 0 || vDeltaR == 0) {
        mMixPrev.vR = rParam.vR;
    } else {
        mMixPrev.vR += vDeltaR * AX_SAMPLES_PER_FRAME;
    }

    if (rParam.vS == 0 || vDeltaS == 0) {
        mMixPrev.vS = rParam.vS;
    } else {
        mMixPrev.vS += vDeltaS * AX_SAMPLES_PER_FRAME;
    }

    if (rParam.vAuxAL == 0 || vDeltaAuxAL == 0) {
        mMixPrev.vAuxAL = rParam.vAuxAL;
    } else {
        mMixPrev.vAuxAL += vDeltaAuxAL * AX_SAMPLES_PER_FRAME;
    }

    if (rParam.vAuxAR == 0 || vDeltaAuxAR == 0) {
        mMixPrev.vAuxAR = rParam.vAuxAR;
    } else {
        mMixPrev.vAuxAR += vDeltaAuxAR * AX_SAMPLES_PER_FRAME;
    }

    if (rParam.vAuxAS == 0 || vDeltaAuxAS == 0) {
        mMixPrev.vAuxAS = rParam.vAuxAS;
    } else {
        mMixPrev.vAuxAS += vDeltaAuxAS * AX_SAMPLES_PER_FRAME;
    }

    if (rParam.vAuxBL == 0 || vDeltaAuxBL == 0) {
        mMixPrev.vAuxBL = rParam.vAuxBL;
    } else {
        mMixPrev.vAuxBL += vDeltaAuxBL * AX_SAMPLES_PER_FRAME;
    }

    if (rParam.vAuxBR == 0 || vDeltaAuxBR == 0) {
        mMixPrev.vAuxBR = rParam.vAuxBR;
    } else {
        mMixPrev.vAuxBR += vDeltaAuxBR * AX_SAMPLES_PER_FRAME;
    }

    if (rParam.vAuxBS == 0 || vDeltaAuxBS == 0) {
        mMixPrev.vAuxBS = rParam.vAuxBS;
    } else {
        mMixPrev.vAuxBS += vDeltaAuxBS * AX_SAMPLES_PER_FRAME;
    }

    if (rParam.vAuxCL == 0 || vDeltaAuxCL == 0) {
        mMixPrev.vAuxCL = rParam.vAuxCL;
    } else {
        mMixPrev.vAuxCL += vDeltaAuxCL * AX_SAMPLES_PER_FRAME;
    }

    if (rParam.vAuxCR == 0 || vDeltaAuxCR == 0) {
        mMixPrev.vAuxCR = rParam.vAuxCR;
    } else {
        mMixPrev.vAuxCR += vDeltaAuxCR * AX_SAMPLES_PER_FRAME;
    }

    if (rParam.vAuxCS == 0 || vDeltaAuxCS == 0) {
        mMixPrev.vAuxCS = rParam.vAuxCS;
    } else {
        mMixPrev.vAuxCS += vDeltaAuxCS * AX_SAMPLES_PER_FRAME;
    }

    return needUpdate;
}

void AxVoice::SetSrc(f32 ratio, bool initial) {
    ut::AutoInterruptLock lock;

    if (initial) {
        ratio = GetDspRatio(ratio);
        u32 ratioBits = 65536 * ratio;

        AXPBSRC src;

        src.ratioHi = ratioBits >> 16;
        src.ratioLo = ratioBits;

        src.currentAddressFrac = 0;

        src.last_samples[0] = 0;
        src.last_samples[1] = 0;
        src.last_samples[2] = 0;
        src.last_samples[3] = 0;

        AXSetVoiceSrc(mVpb, &src);
    } else {
        ratio = GetDspRatio(ratio);
        AXSetVoiceSrcRatio(mVpb, ratio);
    }
}

bool AxVoice::SetVe(f32 volume, f32 initVolume) {
    ut::AutoInterruptLock lock;

    u16 currentVolume;
    u16 targetVolume;

    if (mFirstVeUpdateFlag || !IsRun()) {
        currentVolume = 32767 * initVolume;
        targetVolume = 32767 * volume;

        mFirstVeUpdateFlag = false;
        mVolumePrev = currentVolume;
    } else {
        currentVolume = mVolumePrev;
        targetVolume = 32767 * volume;
    }

    AXPBVE ve;
    ve.currentVolume = currentVolume;

    int deltaOut;
    s16 deltaIn = (targetVolume - currentVolume) / AX_SAMPLES_PER_FRAME;
    int predIn = currentVolume + (deltaIn * AX_SAMPLES_PER_FRAME);
    deltaOut = deltaIn + (deltaIn > 0 ? 1 : 0);
    int predOut =
        currentVolume + ((deltaOut != 0 ? 1 : -1) * AX_SAMPLES_PER_FRAME);

    if (predOut > 32767 || predOut < 0) {
        ve.currentDelta = deltaIn;
    } else if (ut::Abs(targetVolume - predIn) <
               ut::Abs(targetVolume - predOut)) {
        ve.currentDelta = deltaIn;
    } else {
        ve.currentDelta = deltaOut != 0 ? 1 : -1;
    }

    AXSetVoiceVe(mVpb, &ve);

    if (ve.currentDelta == 0) {
        mVolumePrev = targetVolume;
    } else {
        mVolumePrev += ve.currentDelta * AX_SAMPLES_PER_FRAME;
    }

    return targetVolume != currentVolume;
}

void AxVoice::SetLpf(u16 freq) {
    ut::AutoInterruptLock lock;

    if (freq >= 16000) {
        AXPBLPF lpf;
        lpf.on = 0;
        lpf.yn1 = 0;

        ut::AutoInterruptLock lock2;
        AXSetVoiceLpf(mVpb, &lpf);
    } else if (mVpb->pb.lpf.on == AX_PB_LPF_ON) {
        u16 a0, b0;
        AXGetLpfCoefs(freq, &a0, &b0);
        AXSetVoiceLpfCoefs(mVpb, a0, b0);
    } else {
        AXPBLPF lpf;
        lpf.on = AX_PB_LPF_ON;
        lpf.yn1 = 0;
        AXGetLpfCoefs(freq, &lpf.a0, &lpf.b0);
        AXSetVoiceLpf(mVpb, &lpf);
    }
}

} // namespace detail
} // namespace snd
} // namespace nw4r
