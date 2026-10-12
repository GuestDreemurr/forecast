#ifndef NW4R_SND_BASIC_PLAYER_H
#define NW4R_SND_BASIC_PLAYER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_Types.h>

#include <nw4r/ut.h>

#include <revolution/WPAD.h>

namespace nw4r {
namespace snd {
namespace detail {

class BasicPlayer {
public:
    BasicPlayer();
    virtual ~BasicPlayer() {} // at 0x8

    virtual bool Start() = 0;           // at 0xC
    virtual void Stop() = 0;            // at 0x10
    virtual void Pause(bool flag) = 0;  // at 0x14
    virtual bool IsActive() const = 0;   // at 0x18
    virtual bool IsPrepared() const = 0; // at 0x1C
    virtual bool IsStarted() const = 0;  // at 0x20
    virtual bool IsPause() const = 0;    // at 0x24

    void InitParam();

    u32 GetId() const {
        return mId;
    }
    void SetId(u32 id) {
        mId = id;
    }

    f32 GetVolume() const {
        return mVolume;
    }
    void SetVolume(f32 volume) {
        mVolume = volume;
    }

    f32 GetPitch() const {
        return mPitch;
    }
    void SetPitch(f32 pitch) {
        mPitch = pitch;
    }

    f32 GetPan() const {
        return mPan;
    }
    void SetPan(f32 pan) {
        mPan = pan;
    }

    f32 GetSurroundPan() const {
        return mSurroundPan;
    }
    void SetSurroundPan(f32 pan) {
        mSurroundPan = pan;
    }

    // Pan values from the ambient parameter are kept apart in this version
    f32 GetAmbientPan() const {
        return mAmbientPan;
    }
    void SetAmbientPan(f32 pan) {
        mAmbientPan = pan;
    }

    f32 GetAmbientSurroundPan() const {
        return mAmbientSurroundPan;
    }
    void SetAmbientSurroundPan(f32 pan) {
        mAmbientSurroundPan = pan;
    }

    f32 GetLpfFreq() const {
        return mLpfFreq;
    }
    void SetLpfFreq(f32 freq) {
        mLpfFreq = freq;
    }

    int GetOutputLine() const {
        return mOutputLine;
    }
    void SetOutputLine(int flags) {
        mOutputLine = flags;
    }

    f32 GetMainOutVolume() const {
        return mMainOutVolume;
    }
    void SetMainOutVolume(f32 volume) {
        mMainOutVolume = volume;
    }

    f32 GetMainSend() const {
        return mMainSend;
    }
    void SetMainSend(f32 send) {
        mMainSend = send;
    }

    void SetFxSend(AuxBus bus, f32 send);
    f32 GetFxSend(AuxBus bus) const;

    void SetRemoteOutVolume(int remote, f32 volume);
    f32 GetRemoteOutVolume(int remote) const;

    f32 GetRemoteSend(int remote) const;
    f32 GetRemoteFxSend(int remote) const;

private:
    u32 mId; // at 0x4

    f32 mVolume;      // at 0x8
    f32 mPitch;       // at 0xC
    f32 mPan;         // at 0x10
    f32 mSurroundPan; // at 0x14
    f32 mAmbientPan;         // at 0x18
    f32 mAmbientSurroundPan; // at 0x1C
    f32 mLpfFreq;     // at 0x20
    char UNK_0x24[0x4];

    int mOutputLine;                            // at 0x28
    f32 mMainOutVolume;                         // at 0x2C
    f32 mMainSend;                              // at 0x30
    f32 mFxSend[AUX_BUS_NUM];                   // at 0x34
    f32 mRemoteOutVolume[WPAD_MAX_CONTROLLERS]; // at 0x40
    f32 mRemoteSend[WPAD_MAX_CONTROLLERS];      // at 0x50
    f32 mRemoteFxSend[WPAD_MAX_CONTROLLERS];    // at 0x60
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
