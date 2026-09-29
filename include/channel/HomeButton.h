#ifndef CHANNEL_HOME_BUTTON_H
#define CHANNEL_HOME_BUTTON_H
#include <types.h>

#include <revolution/MEM.h>

// Setup data handed to the HOME Button menu library
struct HomeButtonInfo {
    void* layoutBuf;       // at 0x0
    void* speakerSeBuf;    // at 0x4
    void* msgBuf;          // at 0x8
    void* configBuf;       // at 0xC
    void* workBuf;         // at 0x10
    s32 unk14;             // at 0x14
    s32 unk18;             // at 0x18
    s32 language;          // at 0x1C
    s32 unk20;             // at 0x20
    s32 unk24;             // at 0x24
    u32 configBufSize;     // at 0x28
    u32 workBufSize;       // at 0x2C
    f32 unk30;             // at 0x30
    f32 unk34;             // at 0x34
    f32 unk38;             // at 0x38
    void* externalWorkBuf; // at 0x3C
};

enum HomeButtonResult {
    HOME_BUTTON_RESULT_NONE,
    HOME_BUTTON_RESULT_1,
    HOME_BUTTON_RESULT_2,
    HOME_BUTTON_RESULT_3,
};

class HomeButton {
public:
    HomeButton(u32 manualContent, const char* manualPath, const char* manualPage,
               MEMAllocator* allocator1, MEMAllocator* allocator2, void* workBuf);
    ~HomeButton();

    void Init();
    s32 Calc();
    void Draw();
    BOOL OpenManual();
    void OnReset();

    BOOL IsOpen() const {
        return mIsActive || mOpenManual;
    }

private:
    void CheckHeaps();

public:
    HomeButtonInfo* mInfo;      // at 0x0
    void* mSoundData;           // at 0x4
    void* mSoundBuffer;         // at 0x8
    u8 mIsReady;                // at 0xC
    u8 mIsActive;               // at 0xD
    u8 unkE;                    // at 0xE
    u8 unkF;                    // at 0xF
    u8 mOpenManual;             // at 0x10
    u8 mInManual;               // at 0x11
    u8 mResetRequested;         // at 0x12
    s32 mResult;                // at 0x14
    s32 mFrameCount;            // at 0x18
    u32 mManualContent;         // at 0x1C
    const char* mManualPath;    // at 0x20
    u8 unk24;                   // at 0x24
    const char* mManualPage;    // at 0x28
    char mManualPageBuf[0x200]; // at 0x2C
    MEMAllocator* mAllocator1;  // at 0x22C
    MEMAllocator* mAllocator2;  // at 0x230
};

#endif
