// d_scene.cpp: base class of the channel's scenes and the resources they share
#include <channel/SceneBase.h>
#include <channel/Color.h>
#include <channel/Fade.h>
#include <channel/HomeButton.h>
#include <channel/LayoutButton.h>
#include <channel/PointerHistory.h>
#include <channel/SimpleGlobe.h>
#include <channel/SimpleModel.h>
#include <channel/WorkerThread.h>
#include <channel/System.h>

#include <revolution/VI.h>
#include <cstring>
#include <nw4r/math.h>
#include <nw4r/ut.h>
#include <nw4r/g3d/g3d_scnroot.h>
#include <nw4r/g3d/g3d_init.h>
#include <revolution/CNT.h>
#include <revolution/CX.h>
#include <revolution/MEM.h>
#include <revolution/OS.h>

extern "C" void ShutdownDownloader(s32 event);
wchar_t* FormatNumber(s32 value, wchar_t* pBuf, s32 digits, BOOL zeroPad);
void WrapHour(s32* pHour);


struct GlyphTexture {
    u32 texture; // at 0x0
    f32 width;   // at 0x4
    f32 height;  // at 0x8
};
extern TPLPalette* gCommonTpl;

struct DragScroll {
    DragScroll();
    ~DragScroll() {}

    void Update();

    u8 unk0[0x34]; // at 0x0
};

void PlaySE(s32 id);
extern nw4r::ut::ResFont* gTimeFont;

extern "C" s32 contentOpenNAND(ContentHandle* handle, const char* path, CNTFileInfo* file);

extern MEMiHeapHead* gSceneHeap1;
extern MEMiHeapHead* gSceneHeap2;
extern u8 gEarthLoading;
extern const char* sEarthPath;
extern u32 gEarthFileSize;
extern u32 gEarthUncompSize;
extern u32 gEarthChunkSize;
extern void* gEarthData;
extern void* gEarthChunkBuf;
extern WorkerThread* gEarthThread;
extern void* gEarthModelData;
extern void* gSysFontBuf;
extern void* gSysFontBuf2;
extern nw4r::ut::ArchiveFont* gSysFont2;
extern nw4r::ut::ArchiveFont* gSysFont;
void UpdateSound();
void CalcSound();
extern "C" void HBMStartBlackOut(void);

static const u32 sLanguageTextures[] = {100, 102, 98, 102, 101, 99, 97};

PointerHistory gPointerHistory;
OSCalendarTime sCalendarTime;
GlyphTexture sGlyphTextures[89];
DragScroll gDragScroll;
LayoutButton* sHoveredButtons[WPAD_MAX_CONTROLLERS];
nw4r::ut::TextWriterBase<wchar_t> gTextWriter;
wchar_t sTextBuf[0x100];
char sNameBuf[0x100];
MEMAllocator gSceneAllocator1;
MEMAllocator gSceneAllocator2;
Color gHighlightColor(140, 180, 180, 255);

HomeButton* gHomeButton;
u32 gSceneFrameCount;
u32 gBlinkState;
s32 gAnimCounter1;
s32 gAnimCounter2;
u8 gFatalRequested;
SimpleModel* gEarthModel;

void RequestFatal() {
    gFatalRequested = TRUE;
}

void SceneBase::Exit(BOOL shutdownNet, s32 event) {
    VISetBlack(TRUE);
    VIFlush();
    VIWaitForRetrace();
    StopRumble(-1, 0);

    if (shutdownNet) {
        ShutdownDownloader(event);
    }
}

BOOL LoadSysFont() {
    void* file = LoadContentFile(5, "wbf1.brfna", -32, NULL, gSceneHeap1);
    if (file == NULL) {
        return TRUE;
    }

    u32 size = nw4r::ut::ArchiveFont::GetRequireBufferSize(file);
    gSysFontBuf = MEMAllocFromAllocator(&gSceneAllocator2, size);
    if (gSysFontBuf == NULL) {
        MEM1Free(file);
        OSPanic("d_scene.cpp", 725, "m_pSysFontBuf\n");
    }

    gSysFont = new nw4r::ut::ArchiveFont;
    if (gSysFont == NULL) {
        OSPanic("d_scene.cpp", 732, "m_pSysFont\n");
    }

    if (!gSysFont->Construct(gSysFontBuf, size, file)) {
        MEM1Free(file);
        OSPanic("d_scene.cpp", 737, "nw4r::ut::ArchiveFont::Construct() failed.\n");
    }

    gSysFont->SetAlternateChar(0xE06B);
    MEMFreeToExpHeap(gSceneHeap1, file);
    return FALSE;
}

void FreeSysFonts() {
    if (gSysFont2 != NULL) {
        gSysFont2->Destroy();
        delete gSysFont2;
        gSysFont2 = NULL;
    }

    if (gSysFontBuf2 != NULL) {
        MEMFreeToAllocator(&gSceneAllocator2, gSysFontBuf2);
        gSysFontBuf2 = NULL;
    }

    if (gSysFont != NULL) {
        gSysFont->Destroy();
        delete gSysFont;
        gSysFont = NULL;
    }

    if (gSysFontBuf != NULL) {
        MEMFreeToAllocator(&gSceneAllocator2, gSysFontBuf);
        gSysFontBuf = NULL;
    }
}

inline void SceneBase::UpdatePointerOverMenu() const {
    gPointerOverMenu = FALSE;
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        BOOL valid = FALSE;
        if (gPointerValid[i][0] && gKPADLatest[i] >= 0) {
            valid = TRUE;
        }

        if (valid && GetPointerY(i) < mMenuBarY) {
            gPointerOverMenu = TRUE;
            break;
        }
    }
}

void SceneBase::Calc() {
    if (gFatalRequested) {
        if (!IsState(&SceneBase::StateFatal)) {
            ChangeState(&SceneBase::StateFatal);
        } else {
            switch (mStatePhase) {
            case 0: {
                mStatePhase++;
                GXColor clear = {0, 0, 0, 0};
                GXSetCopyClear(clear, 0xFFFFFF);
                StartFade(2, 20, 0, 0);
                break;
            }
            case -1:
                break;
            default:
                gNextScene = 'FATL';
                break;
            }
        }
        return;
    }

    if (OSGetResetButtonState()) {
        if (gHomeButton->IsOpen()) {
            HBMStartBlackOut();
        } else {
            Exit(TRUE, 4);
            Restart();
        }
    }

    nw4r::g3d::G3dReset();
    OSTicksToCalendarTime(OSGetTime(), &sCalendarTime);
    gSceneFrameCount++;
    unk20();

    if (gEarthLoading && gEarthModelData != NULL) {
        if (gEarthChunkBuf != NULL) {
            MEMFreeToExpHeap(gSceneHeap2, gEarthChunkBuf);
            gEarthChunkBuf = NULL;
        }

        if (gEarthModel == NULL) {
            gEarthModel = new (-32) SimpleModel(gEarthModelData);
        }
    }

    UpdatePointerOverMenu();

    if (mState) {
        (this->*mState)();
    }

    if (!gFatalRequested) {
        UpdateMenuFade();

        u32 frame = gSceneFrameCount;
        unkA0 = (frame >> 8) & 1;
        if ((frame & 31) == 0) {
            gBlinkState ^= 1;
        }

        if ((frame & 15) == 0) {
            if (++gAnimCounter1 > 2) {
                gAnimCounter1 = 0;
            }
            if (++gAnimCounter2 > 2) {
                gAnimCounter2 = 0;
            }
        }

        if (gFade != NULL) {
            gFade->Calc();
        }
        if (gFade2 != NULL) {
            gFade2->Calc();
        }

        unk2C();

        if (gSound != NULL) {
            CalcSound();
        }
    }
}

void SceneBase::unk20() {
    if (gSound != NULL) {
        UpdateSound();
    }
}

void SceneBase::unk2C() {
    if (gEarthModel != NULL) {
        gGlobeAlpha -= 6;
        if (gGlobeAlpha < 0) {
            gGlobeAlpha = 0;
        }
    } else {
        gGlobeAlpha = 255;
    }
}

void SceneBase::Draw() {
    if (!gFatalRequested) {
        (this->*mDrawFunc)();

        if (!gHomeButton->IsOpen()) {
            unk3C();
        }
    }
}

void SceneBase::DrawOverlay() {
    if (!gFatalRequested) {
        if (gFade != NULL) {
            gFade->Draw();
        }
        if (gFade2 != NULL) {
            gFade2->Draw();
        }

        unk38();
        gHomeButton->Draw();
    }
}

void SceneBase::unk38() {}

void SceneBase::unk3C() {
    SetCursor(-1, 1);
}

void SceneBase::DrawTimeJP() {
    if (mClockAlpha != 0) {
        wchar_t* p = FormatNumber(sCalendarTime.hour % 12, sTextBuf, 2, FALSE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::DrawTimeUS() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour % 12;
        if (hour == 0) {
            hour = 12;
        }
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, FALSE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
        f32 width = writer.CalcStringWidth(sTextBuf);
        writer.SetScale(0.75f);
        writer.SetCursor(mClockX + width, mClockY + mAmPmOffsetY);
        if (sCalendarTime.hour < 12) {
            writer.Print(L" a.m.");
        } else {
            writer.Print(L" p.m.");
        }
    }
}

void SceneBase::DrawTimeEN() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour;
        WrapHour(&hour);
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, TRUE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::DrawTimeDE() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour;
        WrapHour(&hour);
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, TRUE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::DrawTimeFR() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour;
        WrapHour(&hour);
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, TRUE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::DrawTimeES() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour;
        WrapHour(&hour);
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, TRUE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::DrawTimeIT() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour;
        WrapHour(&hour);
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, TRUE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::DrawTimeNL() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour;
        WrapHour(&hour);
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, TRUE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::unk40() {}

void SceneBase::UpdateMenuFade() {
    if (gPointerOverMenu || gMenuVisible) {
        if (mClockAlpha != 0) {
            mClockAlpha -= 20;
            if (mClockAlpha < 0) {
                mClockAlpha = 0;
            }
        }

        gMenuBrightness += 0.1f;
        if (gMenuBrightness > 1.0f) {
            gMenuBrightness = 1.0f;
        }
    } else {
        if (mClockAlpha != 255) {
            mClockAlpha += 20;
            if (mClockAlpha > 255) {
                mClockAlpha = 255;
            }
        }

        gMenuBrightness -= 0.1f;
        if (gMenuBrightness < 0.2f) {
            gMenuBrightness = 0.2f;
        }
    }
}

BOOL SceneBase::StateMain() {
    switch (mStatePhase) {
    case 0:
        mStatePhase++;
        break;
    case -1:
        break;
    default:
        if (gReturnToMenuRequested) {
            ChangeState(&SceneBase::StateReturnToMenu);
            return TRUE;
        }

        gHomeButton->unkE = gEarthModel != NULL || !gEarthLoading;
        gHomeButton->unkF = gEarthLoading;

        switch (gHomeButton->Calc()) {
        case 1:
            ChangeState(&SceneBase::StateReturnToMenu);
            return TRUE;
        case 2:
            ChangeState(&SceneBase::StateReset);
            break;
        case 3:
            gFatalRequested = TRUE;
            break;
        case 0:
            if (gHomeButton->IsOpen()) {
                unk28();
            } else {
                gPointerHistory.Update();
                unk24();
            }
            break;
        }
        break;
    }

    return TRUE;
}

void SceneBase::unk28() {}

void SceneBase::unk24() {}

BOOL SceneBase::StateReset() {
    switch (mStatePhase) {
    case -1:
        break;
    case 0:
        Exit(TRUE, 4);
        Restart();
        break;
    case 1:
        if (unk48()) {
            gFade->FadeOut(30);
            if (gReturnToMenuRequested) {
                ChangeState(&SceneBase::StateReturnToMenu);
            } else {
                mStatePhase = 2;
            }
        }
        break;
    case 2:
        if (gFade->mFading == 0) {
            mStatePhase = 3;
        }
        break;
    case 3:
        Exit(TRUE, 4);
        Restart();
        break;
    }

    return TRUE;
}

BOOL SceneBase::StateReturnToMenu() {
    switch (mStatePhase) {
    case -1:
        break;
    case 0:
        if (unk44()) {
            gFade->FadeOut(30);
            mStatePhase = 2;
        } else {
            mStatePhase = 1;
        }
        break;
    case 1:
        if (unk48()) {
            gFade->FadeOut(30);
            mStatePhase = 2;
        }
        break;
    case 2:
    default:
        if (gFade->mFading == 0) {
            if (!gEarthLoading) {
                Exit(TRUE, 5);
                ReturnToMenu();
            } else if (gEarthModel != NULL) {
                Exit(TRUE, 5);
                ReturnToMenu();
            }
        }
        break;
    }

    return TRUE;
}

BOOL SceneBase::StateFatal() {
    switch (mStatePhase) {
    case 0:
        mStatePhase++;
        GXColor clear = {0, 0, 0, 0};
        GXSetCopyClear(clear, 0xFFFFFF);
        StartFade(FADE_CAPTURE_BRIGHTNESS, 20, FADE_NONE, 0);
        break;
    case -1:
        break;
    default:
        gNextScene = SCENE_FATAL;
        break;
    }

    return TRUE;
}

static void* EarthLoadThread(void* arg);

BOOL LoadEarthModel() {
    CNTFileInfo file;
    u8 header[32] ATTRIBUTE_ALIGN(32);
    s32 result;

    gEarthLoading = TRUE;

    result = contentOpenNAND(&gContentHandles[6], sEarthPath, &file);
    switch (result) {
    case 0:
        gEarthFileSize = (contentGetLengthNAND(&file) + 31) & ~31;
        result = contentReadNAND(&file, header, sizeof(header), 0);
        contentCloseNAND(&file);
        if (result == 0) {
            OSReport("Error!! (%s) CNTRead() failed. %d\n", sEarthPath, result);
            return FALSE;
        }

        gEarthUncompSize = CXGetUncompressedSize(header);
        break;
    default:
        OSReport("Error!! (%s) CNTOpen() failed. %d\n", sEarthPath, result);
        return FALSE;
    }

    gEarthChunkSize = 0x10000;
    gEarthData = MEMAllocFromExpHeapEx(gSceneHeap2, gEarthUncompSize, -32);
    gEarthChunkBuf = MEMAllocFromExpHeapEx(gSceneHeap2, gEarthChunkSize, -32);

    if (gEarthThread == NULL) {
        gEarthThread = new WorkerThread(EarthLoadThread);
        if (gEarthThread == NULL) {
            OSPanic("d_scene.cpp", 1699, "\x83\x81\x83\x82\x83\x8A\x82\xAA\x82\xC8\x82\xA2\x81\x49\x81\x49\n");
            return FALSE;
        }
    } else {
        gEarthThread->Restart(EarthLoadThread);
    }

    return TRUE;
}

static void* EarthLoadThread(void* arg) {
    CNTFileInfo file;
    CXUncompContextLZ ctx;
    s32 result;

    result = contentOpenNAND(&gContentHandles[6], sEarthPath, &file);
    if (result == 0) {
        CXInitUncompContextLZ(&ctx, gEarthData);

        for (u32 offset = 0; offset < gEarthFileSize; offset += gEarthChunkSize) {
            u32 size = gEarthFileSize - offset;
            if (size > gEarthChunkSize) {
                size = gEarthChunkSize;
            }

            result = contentReadNAND(&file, gEarthChunkBuf, size, offset);
            if (result == 0) {
                contentCloseNAND(&file);
                OSReport("Error!! (%s) CNTRead() failed. %d\n", sEarthPath, result);
                gFatalRequested = TRUE;
                return NULL;
            }

            CXReadUncompLZ(&ctx, gEarthChunkBuf, size);
        }

        contentCloseNAND(&file);

        BOOL notFinished = ctx.destCount > 0 || ctx.headerSize > 0;
        if (notFinished) {
            OSReport("CXIsFinisiedUncompLZ() is false.");
            gFatalRequested = TRUE;
            return NULL;
        }
    } else {
        OSReport("Error!! (%s) CNTOpen() failed. %d\n", sEarthPath, result);
        gFatalRequested = TRUE;
        return NULL;
    }

    gEarthModelData = gEarthData;
    gEarthData = NULL;
    return NULL;
}

BOOL FreeEarthModel() {
    if (gEarthLoading) {
        if (gEarthModel != NULL) {
            if (gEarthModel != NULL) {
                if (gSimpleGlobe != NULL) {
                    if (gSimpleGlobe->mScnRoot != NULL) {
                        gSimpleGlobe->mScnRoot->Clear();
                    }
                    gSimpleGlobe->Calc();
                }

                delete gEarthModel;
                gEarthModel = NULL;
            }

            if (gEarthModelData != NULL) {
                MEMFreeToExpHeap(gSceneHeap2, gEarthModelData);
                gEarthModelData = NULL;
            }

            gEarthLoading = FALSE;
            return TRUE;
        }

        return FALSE;
    }

    return TRUE;
}

void UpdateButtons(ButtonGroup* group, s32 hoverSound) {
    f32 width = GetScreenWidth();
    f32 halfWidth43 = 0.5f * 608.0f;
    f32 scale = 608.0f / width;
    f32 halfWidth = 0.5f * width;
    f32 halfHeight = 0.5f * gRenderMode.efbHeight;

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        BOOL valid = FALSE;
        if (gPointerValid[i][0] && gKPADLatest[i] >= 0) {
            valid = TRUE;
        }

        if (valid) {
            f32 x;
            if (gWidescreen) {
                x = scale * (gPointerX[i][0] - halfWidth);
            } else {
                x = gPointerX[i][0] - halfWidth43;
            }
            f32 y = halfHeight - gPointerY[i][0];

            if (x < -halfWidth43) {
                x = -halfWidth43;
            } else if (x > halfWidth43) {
                x = halfWidth43;
            }

            if (y < -halfHeight) {
                y = -halfHeight;
            } else if (y > halfHeight) {
                y = halfHeight;
            }

            LayoutButton* button = group->HitTest(x, y);
            if (sHoveredButtons[i] != button) {
                if (sHoveredButtons[i] != NULL) {
                    sHoveredButtons[i]->Release();
                    sHoveredButtons[i]->mHeld = FALSE;
                }

                sHoveredButtons[i] = button;
                if (button != NULL && !button->IsInactive()) {
                    PlaySE(hoverSound);
                    StartRumble(i, 3, 20);
                }
            }

            if (sHoveredButtons[i] != NULL) {
                if (gRelease[i] & WPAD_BUTTON_A) {
                    sHoveredButtons[i]->Release();
                    sHoveredButtons[i]->mHeld = FALSE;
                }
                sHoveredButtons[i]->Hover();
            }
        }
    }
}

void ClearHoveredButtons() {
    sHoveredButtons[0] = NULL;
    sHoveredButtons[1] = NULL;
    sHoveredButtons[2] = NULL;
    sHoveredButtons[3] = NULL;
}

s32 CheckButtonHeld(const char* name, u16 buttons) {
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        BOOL valid = FALSE;
        if (gPointerValid[i][0] && gKPADLatest[i] >= 0) {
            valid = TRUE;
        }

        if (valid) {
            LayoutButton* button = sHoveredButtons[i];
            if (button != NULL && !button->mLocked && button->IsName(name)) {
                u32 pressed;
                if (button->mHeld) {
                    pressed = buttons & gRepeatSlowButtons[i];
                } else {
                    pressed = buttons & gTrig[i];
                }

                if (pressed) {
                    sHoveredButtons[i]->mHeld = TRUE;
                    sHoveredButtons[i]->Press(FALSE);
                    return i;
                }
            }
        }
    }

    return -1;
}

s32 CheckButtonPressed(const char* name, u16 buttons) {
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        BOOL valid = FALSE;
        if (gPointerValid[i][0] && gKPADLatest[i] >= 0) {
            valid = TRUE;
        }

        if (valid) {
            LayoutButton* button = sHoveredButtons[i];
            if (button != NULL && (buttons & gTrig[i]) && !button->mLocked &&
                button->IsName(name)) {
                sHoveredButtons[i]->Press(FALSE);
                return i;
            }
        }
    }

    return -1;
}

void ToDegrees(u16 lon, u16 lat, Vec2* out) {
    out->y = (f32)lat * (360.0f / 65536.0f);
    out->x = (f32)(s16)lon * (360.0f / 65536.0f);
}

static const wchar_t sDateGlyphs[] = {
    L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L'-',
    0x65E5, 0x6708, 0x706B, 0x6C34, 0x6728, 0x91D1, 0x571F,
};

static inline int GetDateGlyph(wchar_t c) {
    for (int i = 0; i < (int)(sizeof(sDateGlyphs) / sizeof(sDateGlyphs[0])); i++) {
        if (c == sDateGlyphs[i]) {
            return i;
        }
    }
    return -1;
}

static const wchar_t sNumGlyphs[33] = {
    L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L':', L'(', L')', L'-', L'+',
};

static inline int GetNumGlyph(wchar_t c) {
    for (int i = 0; i < (int)(sizeof(sNumGlyphs) / sizeof(sNumGlyphs[0])); i++) {
        if (c == sNumGlyphs[i]) {
            return i + 18;
        }
    }
    return -1;
}

static const wchar_t sTempGlyphs[47] = {
    L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L'-', L'C', L'F', 0xFF9F,
};

static inline int GetTempGlyph(wchar_t c) {
    for (int i = 0; i < (int)(sizeof(sTempGlyphs) / sizeof(sTempGlyphs[0])); i++) {
        if (c == sTempGlyphs[i]) {
            return i + 33;
        }
    }
    return -1;
}

static inline BOOL IsNearZero(f32 x) {
    return x < 0.0008f && x > -0.0008f;
}

static inline void DrawGlyph(int index, const Vec2& at, f32 scaleX, f32 scaleY) {
    f32 halfW = 0.5f * sGlyphTextures[index].width;
    f32 halfH = 0.5f * sGlyphTextures[index].height;
    f32 offX = halfW * scaleX;
    f32 offY = halfH * scaleY;
    Vec pos;
    pos.x = at.x - offX;
    pos.y = at.y - offY;
    pos.z = 0.0f;
    DrawTextureAt(gCommonTpl, sGlyphTextures[index].texture, scaleX, scaleY, &pos);
}

f32 CalcDateWidth(const wchar_t* str) {
    u32 len = wcslen(str);
    f32 width = 0.0f;

    for (u32 i = 0; i < len; i++, str++) {
        int glyph = GetDateGlyph(*str);
        if (glyph >= 0) {
            width += sGlyphTextures[glyph].width;
        }
    }

    return width;
}

void DrawDateCentered(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
                      const GXColor* shadowColor) {
    u32 len = wcslen(str);
    f32 width = 0.0f;
    const wchar_t* p = str;

    for (u32 i = 0; i < len; i++, p++) {
        int glyph = GetDateGlyph(*p);
        if (glyph >= 0) {
            width += sGlyphTextures[glyph].width;
        }
    }

    f32 halfWidth = 0.5f * width;
    int first = GetDateGlyph(*str);
    Vec2 start;
    start.x = pos->x - scaleX * (halfWidth - 0.5f * sGlyphTextures[first].width);
    start.y = pos->y;
    DrawDate(str, &start, scaleX, scaleY, color, shadowColor);
}

void DrawDate(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
              const GXColor* shadowColor) {
    Vec2 main = *pos;
    Vec2 shadow;
    shadow.y = 2.0f + pos->y;
    shadow.x = 2.0f + pos->x;

    u32 len = wcslen(str);
    f32 prevWidth;

    for (u32 i = 0; i < len; i++, str++) {
        int index = GetDateGlyph(*str);
        if (index >= 0) {
            if (i != 0) {
                f32 advance = scaleX * (-1.0f + (0.5f * prevWidth + 0.5f * sGlyphTextures[index].width));
                main.x += advance;
                shadow.x += advance;
            }

            GXSetTevColor(GX_TEVREG0, *shadowColor);
            DrawGlyph(index, shadow, scaleX, scaleY);
            GXSetTevColor(GX_TEVREG0, *color);
            DrawGlyph(index, main, scaleX, scaleY);

            prevWidth = sGlyphTextures[index].width;
        }
    }
}

f32 CalcNumWidth(const wchar_t* str, f32 spacing) {
    u32 len = wcslen(str);
    f32 width = 0.0f;

    for (u32 i = 0; i < len; i++, str++) {
        int glyph = GetNumGlyph(*str);
        if (glyph >= 0) {
            width += sGlyphTextures[glyph].width;
        }
    }

    if (!IsNearZero(spacing)) {
        width += spacing * (len - 1);
    }

    return width;
}

void DrawNumCentered(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, f32 spacing, const GXColor* color,
                     const GXColor* color2) {
    u32 len = wcslen(str);
    f32 width = 0.0f;
    const wchar_t* p = str;

    for (u32 i = 0; i < len; i++, p++) {
        int glyph = GetNumGlyph(*p);
        if (glyph >= 0) {
            width += sGlyphTextures[glyph].width;
        }
    }

    if (!IsNearZero(spacing)) {
        width += spacing * (len - 1);
    }

    f32 halfWidth = 0.5f * width;
    int first = GetNumGlyph(*str);
    Vec2 start;
    start.x = pos->x - scaleX * (halfWidth - 0.5f * sGlyphTextures[first].width);
    start.y = pos->y;
    DrawNum(str, &start, scaleX, scaleY, spacing, color, color2);
}

void DrawNum(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, f32 spacing, const GXColor* color,
             const GXColor* color2) {
    Vec2 cur = *pos;
    u32 len = wcslen(str);

    if (IsNearZero(spacing)) {
        spacing = 0.0f;
    }

    GXSetTevColor(GX_TEVREG0, *color);
    GXSetTevColor(GX_TEVREG1, *color2);

    f32 prevWidth;

    for (u32 i = 0; i < len; i++, str++) {
        int index = GetNumGlyph(*str);
        if (index >= 0) {
            if (i != 0) {
                cur.x += scaleX * (spacing + 0.5f * (prevWidth + sGlyphTextures[index].width));
            }

            DrawGlyph(index, cur, scaleX, scaleY);
            prevWidth = sGlyphTextures[index].width;
        }
    }
}

void DrawNumRightAligned(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
                         const GXColor* color2) {
    Vec2 cur = *pos;
    u32 len = wcslen(str);
    f32 narrowSpacing = -3.0f * scaleX;
    f32 wideSpacing = -6.0f * scaleX;
    f32 parenOffset = 2.0f * scaleX;

    GXSetTevColor(GX_TEVREG0, *color);
    GXSetTevColor(GX_TEVREG1, *color2);

    const wchar_t* p = str + (len - 1);
    f32 prevWidth;
    f32 spacing;

    for (u32 i = 0; i < len; i++, p--) {
        int index = GetNumGlyph(*p);
        if (index >= 0) {
            if (i != 0) {
                cur.x -= spacing + 0.5f * (prevWidth + sGlyphTextures[index].width) * scaleX;
                if (*p == L'(') {
                    cur.x += parenOffset;
                }
            }

            DrawGlyph(index, cur, scaleX, scaleY);
            prevWidth = sGlyphTextures[index].width;

            switch (*p) {
            case L')':
                spacing = wideSpacing;
                break;
            default:
                spacing = narrowSpacing;
                break;
            }
        }
    }
}

f32 CalcTempWidth(const wchar_t* str) {
    u32 len = wcslen(str);
    f32 width = 0.0f;

    for (u32 i = 0; i < len; i++, str++) {
        int glyph = GetTempGlyph(*str);
        if (glyph >= 0) {
            width += sGlyphTextures[glyph].width;
        }
    }

    return width;
}

f32 GetTempGlyphHeight(const wchar_t* str) {
    int glyph = GetTempGlyph(*str);
    if (glyph >= 0) {
        return sGlyphTextures[glyph].height;
    }
    return 0.0f;
}

void DrawTempCentered(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
                      const GXColor* shadowColor) {
    u32 len = wcslen(str);
    f32 width = 0.0f;
    const wchar_t* p = str;

    for (u32 i = 0; i < len; i++, p++) {
        int glyph = GetTempGlyph(*p);
        if (glyph >= 0) {
            width += sGlyphTextures[glyph].width;
        }
    }

    f32 halfWidth = 0.5f * width;
    int first = GetTempGlyph(*str);
    Vec2 start;
    start.x = pos->x - scaleX * (halfWidth - 0.5f * sGlyphTextures[first].width);
    start.y = pos->y;
    DrawTemp(str, &start, scaleX, scaleY, color, shadowColor);
}

void DrawTemp(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
              const GXColor* shadowColor) {
    Vec2 main = *pos;
    Vec2 shadow;
    shadow.y = 2.0f + pos->y;
    shadow.x = 2.0f + pos->x;

    u32 len = wcslen(str);
    f32 prevWidth;

    for (u32 i = 0; i < len; i++, str++) {
        int index = GetTempGlyph(*str);
        if (index >= 0) {
            if (i != 0) {
                f32 advance = scaleX * (0.5f * prevWidth + 0.5f * sGlyphTextures[index].width);
                main.x += advance;
                shadow.x += advance;
            }

            GXSetTevColor(GX_TEVREG0, *shadowColor);
            DrawGlyph(index, shadow, scaleX, scaleY);
            GXSetTevColor(GX_TEVREG0, *color);
            DrawGlyph(index, main, scaleX, scaleY);

            prevWidth = sGlyphTextures[index].width;
        }
    }
}

void UpdateDragScroll() {
    gDragScroll.Update();
}

u32 GetLanguageTexture() {
    return sLanguageTextures[gLanguage];
}

void SceneBase::OnShutdown() {
    Exit(TRUE, 2);
}

void SceneBase::OnPowerButton() {
    if (gHomeButton != NULL) {
        gHomeButton->OnReset();
    }
}

void SceneBase::OnResetButton() {
    if (gHomeButton != NULL) {
        gHomeButton->OnReset();
    }
}

void SceneBase::Init() {}
