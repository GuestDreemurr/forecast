#ifndef CHANNEL_LOOP_SOUND_H
#define CHANNEL_LOOP_SOUND_H
#include <types.h>
#include <nw4r/snd/snd_SoundHandle.h>

// A looping sound that keeps playing only while it is requested every frame
class LoopSound {
public:
    void ClearRequest();
    void Update();
    void Request(u32 id, f32 volume, f32 pitch, f32 pan);

    nw4r::snd::SoundHandle mHandle; // at 0x0
    u8 mRequested;                  // at 0x4
};

#endif
