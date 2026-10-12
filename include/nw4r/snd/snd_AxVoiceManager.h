#ifndef NW4R_SND_AX_VOICE_MANAGER_H
#define NW4R_SND_AX_VOICE_MANAGER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_AxVoice.h>

namespace nw4r {
namespace snd {
namespace detail {

class AxVoiceManager {
public:
    static const int WORK_SIZE_MAX = AX_VOICE_MAX * sizeof(AxVoice) + 0x20;

public:
    static AxVoiceManager& GetInstance();

    u32 GetRequiredMemSize();
    void Setup(void* pBuffer, u32 size);

    AxVoice* AcquireAxVoice(u32 priority, AxVoice::AxVoiceCallback pCallback,
                            void* pArg);
    void FreeAxVoice(AxVoice* pVoice);

    AxVoice* GetAxVoice(u32 index) {
        return &mVoiceTable[index];
    }

private:
    AxVoiceManager() : mInitialized(false) {}

private:
    bool mInitialized;    // at 0x0
    u32 mVoiceCount;      // at 0x4
    AxVoice* mVoiceTable; // at 0x8
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
