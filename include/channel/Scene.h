#ifndef CHANNEL_SCENE_H
#define CHANNEL_SCENE_H
#include <types.h>

class LayoutObj;

// Pointer cursor shared by every scene
class Cursor {
public:
    Cursor();

    void Reset();
    void Calc();
    void Draw();
    void Set(s32 chan, s32 type);
    void UpdateParticles(void* emitterSet, f32 rotation, f32 brightness);

    void* mEffectHeap;       // at 0x0, 128KB MEM2 block for the effect system
    void* mEffectMemory;     // at 0x4, effect memory manager (0x4C bytes)
    void* mEffectData;       // at 0x8, nw4r_defcursor_all01.breff
    void* mEffectTexData;    // at 0xC, nw4r_defcursor_all01.breft
    s32 mSpinCounter[4];     // at 0x10, animation counter for the spinning (type 4) cursor
    s32 mType[4];            // at 0x20, cursor style per controller (set via SetCursor), cleared every frame
    void* mCursorEmitter[4]; // at 0x30
    void* mSpinEmitter[4];   // at 0x40, only for type 4
    void* mShadowEmitter[4]; // at 0x50
    u8 mEffectReady;         // at 0x60, both effect files loaded
    u8 unk61[0x3];           // at 0x61
};

// 'WTH2': the main forecast scene
class WeatherScene {
public:
    WeatherScene();
    virtual ~WeatherScene();           // at 0x8
    virtual void unk0C();              // at 0xC
    virtual void OnShutdown();         // at 0x10
    virtual void OnPowerButton();      // at 0x14
    virtual void OnResetButton();      // at 0x18
    virtual void Init();               // at 0x1C
    virtual void unk20();              // at 0x20
    virtual void unk24();              // at 0x24
    virtual void unk28();              // at 0x28
    virtual void unk2C();              // at 0x2C
    virtual void Draw();               // at 0x30
    virtual void DrawOverlay();        // at 0x34

    u8 unk4[0x16C - 0x4]; // at 0x4
};

// 'FATL': the error screen
class FatalScene {
public:
    FatalScene();
    ~FatalScene();

    void Init();
    void Calc();
    void Draw();

    LayoutObj* mLayout; // at 0x0
    s32 mExitTimer; // at 0x4
};

extern Cursor* gCursor;
extern WeatherScene* gWeatherScene;
extern FatalScene* gFatalScene;

#endif
