#ifndef CHANNEL_SCENE_H
#define CHANNEL_SCENE_H
#include <types.h>

// Pointer cursor shared by every scene
class Cursor {
public:
    Cursor();

    void Reset();
    void Calc();
    void Draw();
    void Set(s32 chan, s32 arg);

    u8 unk0[0x20];  // at 0x0
    s32 unk20;      // at 0x20
    s32 unk24;      // at 0x24
    s32 unk28;      // at 0x28
    s32 unk2C;      // at 0x2C
    u8 unk30[0x30]; // at 0x30
    u8 unk60;       // at 0x60
    u8 unk61[0x3];  // at 0x61
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

    void* mLayout; // at 0x0
    s32 mExitTimer; // at 0x4
};

extern Cursor* gCursor;
extern WeatherScene* gWeatherScene;
extern FatalScene* gFatalScene;

#endif
