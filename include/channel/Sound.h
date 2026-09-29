#ifndef CHANNEL_SOUND_H
#define CHANNEL_SOUND_H
#include <types.h>

namespace nw4r {
namespace snd {
class SoundHandle;
}
}

class Sound {
public:
    ~Sound();
};

void StartSound(nw4r::snd::SoundHandle* handle, u32 id);
void StopSound(nw4r::snd::SoundHandle* handle, s32 fadeFrames);
void SetSoundVolume(nw4r::snd::SoundHandle* handle, f32 volume);
void SetSoundPitch(nw4r::snd::SoundHandle* handle, f32 pitch);
void SetSoundPan(nw4r::snd::SoundHandle* handle, f32 pan);
bool IsSoundPlaying(nw4r::snd::SoundHandle* handle);

#endif
