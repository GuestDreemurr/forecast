#ifndef CHANNEL_SYSTEM_H
#define CHANNEL_SYSTEM_H
#include <types.h>

#include <revolution/CNT/cnt.h>
#include <revolution/GX.h>
#include <revolution/KPAD.h>
#include <revolution/MEM.h>
#include <revolution/MTX.h>
#include <revolution/TPL.h>
#include <revolution/WPAD.h>

#define SCREEN_HEIGHT 456
#define SCREEN_WIDTH_4_3 608
#define SCREEN_WIDTH_16_9 832

#define KPAD_READ_MAX 16
#define CONTENT_HANDLE_MAX 10

// Four-character scene identifiers
#define SCENE_NONE 'NONE'
#define SCENE_WEATHER 'WTH2'
#define SCENE_FATAL 'FATL'

enum FadeType {
    FADE_NONE,
    FADE_TO_BLACK,
    FADE_CAPTURE_BRIGHTNESS,
    FADE_ZOOM_IN,
    FADE_ZOOM_OUT,
};

struct ContentHandle {
    u8 data[0x24];
};

// Heaps and allocators
extern MEMiHeapHead* gMEM1Heap;
extern u32 gMEM1FreeSize;
extern MEMiHeapHead* gMEM2Heap;
extern u32 gMEM2FreeSize;
extern MEMAllocator gMEM1Allocator;
extern MEMAllocator gMEM1Allocator32;
extern MEMAllocator gMEM2Allocator;
extern MEMAllocator gMEM2Allocator32;

// Video
extern void* gGXFifo;
extern GXFifoObj* gGXFifoObj;
extern u32 gXfbSize;
extern void* gXfb1;
extern void* gXfb2;
extern void* gCurrentXfb;
extern GXRenderModeObj gRenderMode;
extern u8 gUnblackNextFrame;
extern u8 gWidescreen;
extern u8 gProgressive;

// Scenes
extern s32 gScene;
extern s32 gNextScene;

// Controllers
extern u8 gPointerZoomBase[WPAD_MAX_CONTROLLERS];
extern u8 gPointerZoomActive[WPAD_MAX_CONTROLLERS];
extern u8 gPointerWasValid[WPAD_MAX_CONTROLLERS];
extern u8 gConnected[WPAD_MAX_CONTROLLERS];
extern u8 gRepeatSlow[WPAD_MAX_CONTROLLERS];
extern u8 gRepeatFast[WPAD_MAX_CONTROLLERS];
extern u32 gHoldAll;
extern u32 gTrigAll;
extern u32 gReleaseAll;
extern u32 gRepeatSlowAll;
extern u32 gRepeatFastAll;
extern u8 gMotorOn[WPAD_MAX_CONTROLLERS];
extern s32 gKPADLatest[WPAD_MAX_CONTROLLERS];
extern KPADStatus gKPADStatus[WPAD_MAX_CONTROLLERS][KPAD_READ_MAX];
extern s32 gKPADReadCount[WPAD_MAX_CONTROLLERS];
extern f32 gPointerX[WPAD_MAX_CONTROLLERS][KPAD_READ_MAX];
extern f32 gPointerY[WPAD_MAX_CONTROLLERS][KPAD_READ_MAX];
extern f32 gPointerDist[WPAD_MAX_CONTROLLERS][KPAD_READ_MAX];
extern f32 gCursorX[WPAD_MAX_CONTROLLERS];
extern f32 gCursorY[WPAD_MAX_CONTROLLERS];
extern f32 gZoom[WPAD_MAX_CONTROLLERS];
extern f32 gZoomBaseDist[WPAD_MAX_CONTROLLERS];
extern u8 gPointerValid[WPAD_MAX_CONTROLLERS][KPAD_READ_MAX];
extern u32 gHold[WPAD_MAX_CONTROLLERS];
extern u32 gTrig[WPAD_MAX_CONTROLLERS];
extern u32 gRelease[WPAD_MAX_CONTROLLERS];
extern Vec2 gHorizon[WPAD_MAX_CONTROLLERS];
extern u32 gRepeatSlowButtons[WPAD_MAX_CONTROLLERS];
extern u32 gRepeatFastButtons[WPAD_MAX_CONTROLLERS];
extern s32 gHoldFrames[WPAD_MAX_CONTROLLERS];
extern s32 gMotorTimer[WPAD_MAX_CONTROLLERS];
extern s32 gMotorCooldown[WPAD_MAX_CONTROLLERS];
extern const char* gMotorPattern[WPAD_MAX_CONTROLLERS];
extern s32 gMotorPatternPos[WPAD_MAX_CONTROLLERS];

// Settings
extern u8 gLanguage;
extern u32 gCountryCode;
extern s32 gRegion;
extern u32 gRandSeed;

// Transitions
extern void* gCaptureTexture;
extern s32 gFadeType;
extern s32 gFadeTimer;
extern s32 gFadeDuration;
extern s32 gFadeNextType;
extern s32 gFadeNextDuration;
extern u8 gFadeCaptured;
extern u8 gFadeCaptureRequest;
extern u8 gFading;
extern u8 gShutdownRequested;
extern u32 gFrameCount;

extern CNTHandleNAND gContentHandles[CONTENT_HANDLE_MAX];

extern u8 gUnk80330B40;
extern s32 gUnk80330B64;
extern u32 gUnk80330B74;

void SystemInit(void);
void SystemCalc(void);
void SystemDraw(void);

void ChangeScene(u32 scene);
void SetDefaultGXState(void);
void SetOrthoProjection(void);
void MakeTransformMtx(const Vec* scale, const Vec2* rotation, const Vec* trans, Mtx out);
s32 GetAreaGroup(void);
u8 GetLanguage(void);
const char* GetLanguageCode(void);
void* LoadContentFile(u32 content, const char* path, s32 align, u32* sizeOut, MEMiHeapHead* heap);
void* LoadCompressedContentFile(u32 content, const char* path, s32 align, u32* sizeOut,
                                MEMiHeapHead* heap);
void DrawLine(const Vec* start, const Vec* end, u8 width, const GXColor& startColor, const GXColor& endColor);
void SetVideoMode(BOOL progressive, BOOL widescreen, BOOL narrow);
void GetTexObj(TPLPalette* palette, u32 id, GXTexObj* texObj);
u32 GetTexWidth(TPLPalette* palette, u32 id);
u32 GetTexHeight(TPLPalette* palette, u32 id);
void DrawTextureAt(TPLPalette* palette, u32 id, f32 scaleX, f32 scaleY, const Vec* pos);
void SetRenderMode(GXRenderModeObj* rmode);
void SetScaledScissor(u32 left, u32 top, u32 width, u32 height);
void StartFade(s32 type, s32 duration, s32 nextType, s32 nextDuration);
void SetCursor(s32 chan, s32 type);
void StartRumble(s32 chan, s32 frames, s32 cooldown);
void StopRumble(s32 chan, s32 cooldown);
void ReturnToMenu(void);
void Restart(void);

void* MEM1Alloc(u32 size, s32 align);
void* MEM2Alloc(u32 size, s32 align);
void MEM1Free(void* block);
void MEM2Free(void* block);

void* operator new(size_t size, s32 align);

static inline s32 GetScreenWidth(void) {
    return gWidescreen ? SCREEN_WIDTH_16_9 : SCREEN_WIDTH_4_3;
}

static inline s32 GetScreenHeight(void) {
    return SCREEN_HEIGHT;
}

static inline f32 GetPointerX(s32 chan) {
    return gPointerX[chan][0];
}

static inline f32 GetPointerY(s32 chan) {
    return gPointerY[chan][0];
}

static inline BOOL IsPointerValid(s32 chan) {
    return gPointerValid[chan][0] && gKPADLatest[chan] >= 0;
}

static inline void SetVec(Vec* v, f32 x, f32 y, f32 z) {
    v->x = x;
    v->y = y;
    v->z = z;
}

#endif
