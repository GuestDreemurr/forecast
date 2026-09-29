// Looping ambient sounds that stop when no longer requested
#include <channel/LoopSound.h>
#include <channel/Sound.h>

void LoopSound::ClearRequest() {
    mRequested = FALSE;
}

void LoopSound::Update() {
    if (!mRequested && IsSoundPlaying(&mHandle)) {
        StopSound(&mHandle, 0);
    }
}

void LoopSound::Request(u32 id, f32 volume, f32 pitch, f32 pan) {
    mRequested = TRUE;

    if (!IsSoundPlaying(&mHandle)) {
        StartSound(&mHandle, id);
        SetSoundVolume(&mHandle, volume);
        SetSoundPitch(&mHandle, pitch);
        SetSoundPan(&mHandle, pan);
    } else {
        SetSoundVolume(&mHandle, volume);
        SetSoundPitch(&mHandle, pitch);
        SetSoundPan(&mHandle, pan);
    }
}
