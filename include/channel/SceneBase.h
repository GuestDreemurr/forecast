#ifndef CHANNEL_SCENE_BASE_H
#define CHANNEL_SCENE_BASE_H
#include <types.h>

class HomeButton;
class SimpleModel;

// Base class of the channel's scenes (d_scene.cpp). Runs a small state machine: each state is a
// member function called with mStatePhase = -1 on exit and counting up from 0 while active.
class SceneBase {
public:
    typedef void (SceneBase::*StateFunc)();

    SceneBase(bool arg);
    virtual ~SceneBase();                    // at 0x8
    virtual void Exit(BOOL shutdownNet, s32 event); // at 0xC
    virtual void OnShutdown();               // at 0x10
    virtual void OnPowerButton();            // at 0x14
    virtual void OnResetButton();            // at 0x18
    virtual void Init();                     // at 0x1C
    virtual void unk20();                    // at 0x20
    virtual void unk24();                    // at 0x24
    virtual void unk28();                    // at 0x28
    virtual void unk2C();                    // at 0x2C
    virtual void Draw();                     // at 0x30
    virtual void DrawOverlay();              // at 0x34
    virtual void unk38();                    // at 0x38
    virtual void unk3C();                    // at 0x3C
    virtual void unk40();                    // at 0x40
    virtual void unk44();                    // at 0x44
    virtual void unk48();                    // at 0x48

    u8 unk4[0x64 - 0x4];  // at 0x4, nw4r::ut::TextWriterBase<wchar_t>
    StateFunc mNextState; // at 0x64
    StateFunc mState;     // at 0x70
    f32 unk7C[7];         // at 0x7C
    s32 mStatePhase;      // at 0x98
    s32 unk9C;            // at 0x9C
    s32 unkA0;            // at 0xA0
    u8 unkA4;             // at 0xA4
    s32 unkA8;            // at 0xA8
};

extern HomeButton* gHomeButton;
extern SimpleModel* gEarthModel;
extern u8 gExitRequested;
extern s32 gGlobeAlpha;

void RequestExit();
u32 GetLanguageTexture();

#endif
