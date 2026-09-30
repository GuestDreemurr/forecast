#ifndef CHANNEL_SCENE_H
#define CHANNEL_SCENE_H
#include <types.h>
#include <channel/SceneBase.h>
#include <channel/WeatherScene.h>

class LayoutObj;
class MemoryManager;
namespace nw4r {
namespace ef {
class Effect;
}
}

// Pointer cursor shared by every scene
class Cursor {
public:
    Cursor();

    void Reset();
    void Calc();
    void Draw();
    void Set(s32 chan, s32 type);
    void UpdateParticles(nw4r::ef::Effect* effect, f32 rotation, f32 brightness);

    void* mEffectHeap;       // at 0x0, 128KB MEM2 block for the effect system
    MemoryManager* mEffectMemory; // at 0x4, nw4r::ef memory manager
    void* mEffectData;       // at 0x8, nw4r_defcursor_all01.breff
    void* mEffectTexData;    // at 0xC, nw4r_defcursor_all01.breft
    s32 mSpinCounter[4];     // at 0x10, animation counter for the spinning (type 4) cursor
    s32 mType[4];            // at 0x20, cursor style per controller (set via SetCursor), cleared every frame
    nw4r::ef::Effect* mCursorEffect[4]; // at 0x30
    nw4r::ef::Effect* mSpinEffect[4];   // at 0x40, only for type 4
    nw4r::ef::Effect* mShadowEffect[4]; // at 0x50
    u8 mEffectReady;         // at 0x60, both effect files loaded
    u8 unk61[0x3];           // at 0x61
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
extern FatalScene* gFatalScene;

#endif
