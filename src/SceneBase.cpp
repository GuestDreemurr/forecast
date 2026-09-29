// d_scene.cpp: base class of the channel's scenes and the resources they share
#include <channel/SceneBase.h>
#include <channel/Fade.h>
#include <channel/HomeButton.h>
#include <channel/SimpleGlobe.h>
#include <channel/SimpleModel.h>
#include <channel/WorkerThread.h>
#include <channel/System.h>

#include <revolution/VI.h>
#include <nw4r/math.h>
#include <nw4r/ut.h>
#include <nw4r/g3d/g3d_scnroot.h>
#include <revolution/CNT.h>
#include <revolution/CX.h>
#include <revolution/MEM.h>
#include <revolution/OS.h>

extern "C" void ShutdownDownloader(s32 event);

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
extern MEMAllocator gSceneAllocator2;
extern void* gSysFontBuf;
extern void* gSysFontBuf2;
extern nw4r::ut::ArchiveFont* gSysFont2;
extern nw4r::ut::ArchiveFont* gSysFont;
void UpdateSound();

static const u32 sLanguageTextures[] = {100, 102, 98, 102, 101, 99, 97};

HomeButton* gHomeButton;
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

void SceneBase::unk40() {}

void SceneBase::UpdateMenuFade() {
    if (gPointerOverMenu || gMenuVisible) {
        if (mMenuShadeAlpha != 0) {
            mMenuShadeAlpha -= 20;
            if (mMenuShadeAlpha < 0) {
                mMenuShadeAlpha = 0;
            }
        }

        gMenuBrightness += 0.1f;
        if (gMenuBrightness > 1.0f) {
            gMenuBrightness = 1.0f;
        }
    } else {
        if (mMenuShadeAlpha != 255) {
            mMenuShadeAlpha += 20;
            if (mMenuShadeAlpha > 255) {
                mMenuShadeAlpha = 255;
            }
        }

        gMenuBrightness -= 0.1f;
        if (gMenuBrightness < 0.2f) {
            gMenuBrightness = 0.2f;
        }
    }
}

void SceneBase::unk28() {}

void SceneBase::unk24() {}

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

void ToDegrees(u16 lon, u16 lat, Vec2* out) {
    out->y = (f32)lat * (360.0f / 65536.0f);
    out->x = (f32)(s16)lon * (360.0f / 65536.0f);
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
