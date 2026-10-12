#include <nw4r/snd.h>

#include <revolution/AX.h>
#include <revolution/OS.h>
#include <revolution/SC.h>

namespace {

static bool sInitialized = false;

} // namespace

namespace nw4r {
namespace snd {

void SoundSystem::InitSoundSystem(s32 soundThreadPriority,
                                  s32 dvdThreadPriority) {
    const int defaultWorkSize = DEFAULT_SOUND_THREAD_STACK_SIZE +
                                DEFAULT_DVD_THREAD_STACK_SIZE +
                                detail::AxVoiceManager::WORK_SIZE_MAX +
                                detail::VoiceManager::WORK_SIZE_MAX +
                                detail::ChannelManager::WORK_SIZE_MAX;

    static u8 defaultSoundSystemWork[defaultWorkSize] ALIGN(32);

    SoundSystemParam param;
    param.soundThreadPriority = soundThreadPriority;
    param.dvdThreadPriority = dvdThreadPriority;

    // @bug This function ignores the specified buffer size
    InitSoundSystem(param, defaultSoundSystemWork,
                    sizeof(defaultSoundSystemWork));
}

void SoundSystem::InitSoundSystem(const SoundSystemParam& rParam, void* pWork,
                                  u32 workSize) {
#pragma unused(workSize)

    if (sInitialized) {
        return;
    }

    sInitialized = true;

    detail::AxManager::GetInstance().Init();

    SCInit();
    while (SCCheckStatus() == SC_STATUS_BUSY) {
        ;
    }

    switch (SCGetSoundMode()) {
    case SC_SND_MONO: {
        detail::AxManager::GetInstance().SetOutputMode(OUTPUT_MODE_MONO);
        break;
    }

    case SC_SND_STEREO: {
        detail::AxManager::GetInstance().SetOutputMode(OUTPUT_MODE_STEREO);
        break;
    }

    case SC_SND_SURROUND: {
        detail::AxManager::GetInstance().SetOutputMode(OUTPUT_MODE_DPL2);
        break;
    }

    default: {
        detail::AxManager::GetInstance().SetOutputMode(OUTPUT_MODE_STEREO);
        break;
    }
    }

    detail::RemoteSpeakerManager::GetInstance().Setup();

    u8* pPtr = static_cast<u8*>(pWork);

    void* pDvdThreadStack = pPtr;
    pPtr += rParam.dvdThreadStackSize;

    void* pSoundThreadStack = pPtr;
    pPtr += rParam.soundThreadStackSize;

    void* pAxVoiceWork = pPtr;
    pPtr += detail::AxVoiceManager::GetInstance().GetRequiredMemSize();

    detail::AxVoiceManager::GetInstance().Setup(
        pAxVoiceWork,
        detail::AxVoiceManager::GetInstance().GetRequiredMemSize());

    void* pVoiceWork = pPtr;
    pPtr += detail::VoiceManager::GetInstance().GetRequiredMemSize();

    detail::VoiceManager::GetInstance().Setup(
        pVoiceWork, detail::VoiceManager::GetInstance().GetRequiredMemSize());

    void* pChannelWork = pPtr;
    pPtr += detail::ChannelManager::GetInstance().GetRequiredMemSize();

    detail::ChannelManager::GetInstance().Setup(
        pChannelWork,
        detail::ChannelManager::GetInstance().GetRequiredMemSize());

    detail::TaskThread::GetInstance().Create(
        rParam.dvdThreadPriority, pDvdThreadStack, rParam.dvdThreadStackSize);

    detail::SoundThread::GetInstance().Create(rParam.soundThreadPriority,
                                              pSoundThreadStack,
                                              rParam.soundThreadStackSize);
}

} // namespace snd
} // namespace nw4r
