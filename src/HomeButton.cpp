#include <channel/HomeButton.h>
#include <channel/System.h>

#include <revolution/GX.h>
#include <revolution/KPAD.h>
#include <revolution/MTX.h>
#include <revolution/NAND.h>
#include <revolution/SC.h>
#include <revolution/VI.h>
#include <revolution/WPAD.h>
#include <cstring>
#include <mem.h>

#define HOME_BUTTON_SOUND_BUFFER_SIZE 0x1D000
#define HOME_BUTTON_WORK_BUFFER_SIZE 0x80000

// Not yet decompiled (channel)
extern "C" void fn_8003ABAC(void* arg0, s32 arg1);
extern "C" s32 fn_8003BF08(void);
extern "C" void fn_8003C054(void);
extern "C" void fn_8003D6D0(void);
extern "C" void fn_8003D9E0(void);

// Not yet decompiled (libraries)
extern "C" void fn_80071FD8(HomeButtonInfo* info);
extern "C" void fn_80072100(void);
extern "C" void fn_80072184(void);
extern "C" s32 fn_800721B0(void* controllers);
extern "C" void fn_800721F4(void);
extern "C" s32 fn_80072320(void);
extern "C" void fn_80072340(u8 widescreen);
extern "C" void fn_80072530(void* soundData, void* soundBuffer, u32 soundBufferSize);
extern "C" void fn_800725EC(void);
extern "C" void fn_80072620(void);
extern "C" void fn_80083A80(MEMAllocator* allocator1, MEMAllocator* allocator2);
extern "C" s32 fn_80083AD8(void);
extern "C" void fn_80083ADC(void);
extern "C" void fn_80083B3C(GXRenderModeObj* rmode1, GXRenderModeObj* rmode2, s32 arg2);
extern "C" s32 fn_80083B44(s32 width, s32 height);
extern "C" void fn_80083B48(s32 arg0);
extern "C" void fn_80083B4C(void);
extern "C" s32 fn_80083B50(u32 size);
extern "C" void fn_80083C44(void);
extern "C" void fn_80083C9C(void* data);
extern "C" const char* fn_80083CA0(void (*callback)(u8, GXRenderModeObj*), const char* page,
                                   s32 chan);
extern "C" void fn_80083CA4(const char* page);
extern "C" void fn_80083CAC(s32 arg0);
extern "C" void fn_80083D04(HomeButtonInfo* info);
extern "C" u32 fn_80110A04(MEMiHeapHead* heap);

extern void* lbl_80330BBC;

struct HomeButtonController {
    KPADStatus* status; // at 0x0
    u8 unk4[0xC];       // at 0x4
};

static const char* sLayoutArchives[] = {
    "HomeButton3/LZ77_homeBtn.arc",     "HomeButton3/LZ77_homeBtn_ENG.arc",
    "HomeButton3/LZ77_homeBtn_GER.arc", "HomeButton3/LZ77_homeBtn_FRA.arc",
    "HomeButton3/LZ77_homeBtn_SPA.arc", "HomeButton3/LZ77_homeBtn_ITA.arc",
    "HomeButton3/LZ77_homeBtn_NED.arc",
};

static void DrawManualFade(u8 alpha, GXRenderModeObj* rmode);

HomeButton::HomeButton(u32 manualContent, const char* manualPath, const char* manualPage,
                       MEMAllocator* allocator1, MEMAllocator* allocator2, void* workBuf) {
    mIsReady = FALSE;
    mSoundBuffer = NULL;
    mSoundData = NULL;

    mInfo = new HomeButtonInfo;
    if (mInfo != NULL) {
        BOOL operaCopied = FALSE;
        const char* layoutPath;
        u32 operaSize;
        void* opera;

        mInfo->layoutBuf = NULL;
        mInfo->speakerSeBuf = NULL;
        mInfo->msgBuf = NULL;
        mInfo->configBuf = NULL;
        mInfo->workBuf = NULL;

        mInfo->language = gLanguage;
        switch (gLanguage) {
        case SC_LANG_JP:
            layoutPath = sLayoutArchives[SC_LANG_JP];
            break;
        case SC_LANG_EN:
            layoutPath = sLayoutArchives[SC_LANG_EN];
            break;
        case SC_LANG_DE:
            layoutPath = sLayoutArchives[SC_LANG_DE];
            break;
        case SC_LANG_FR:
            layoutPath = sLayoutArchives[SC_LANG_FR];
            break;
        case SC_LANG_SP:
            layoutPath = sLayoutArchives[SC_LANG_SP];
            break;
        case SC_LANG_IT:
            layoutPath = sLayoutArchives[SC_LANG_IT];
            break;
        case SC_LANG_NL:
            layoutPath = sLayoutArchives[SC_LANG_NL];
            break;
        default:
            mInfo->language = SC_LANG_EN;
            layoutPath = sLayoutArchives[SC_LANG_EN];
            break;
        }

        mInfo->layoutBuf = LoadCompressedContentFile(4, layoutPath, 32, NULL, gMEM2Heap);
        mInfo->speakerSeBuf =
            LoadCompressedContentFile(4, "HomeButton3/Huf8_SpeakerSe.arc", 32, NULL, gMEM2Heap);
        mInfo->msgBuf = LoadCompressedContentFile(7, "home_nosave.csv.LZ", 32, NULL, gMEM2Heap);
        mInfo->configBuf =
            LoadContentFile(4, "HomeButton3/config.txt", 32, &mInfo->configBufSize, gMEM2Heap);

        if (workBuf != NULL) {
            mInfo->externalWorkBuf = workBuf;
            mInfo->workBuf = NULL;
        } else {
            mInfo->externalWorkBuf = NULL;
            mInfo->workBuf = MEM2Alloc(HOME_BUTTON_WORK_BUFFER_SIZE, 32);
        }
        mInfo->workBufSize = HOME_BUTTON_WORK_BUFFER_SIZE;

        mSoundData =
            LoadCompressedContentFile(4, "HomeButton3/Huf8_HomeButtonSe.brsar", 32, NULL, gMEM2Heap);
        mSoundBuffer = MEM2Alloc(HOME_BUTTON_SOUND_BUFFER_SIZE, 0);

        opera = LoadContentFile(7, "Opera.arc", 32, &operaSize, gMEM1Heap);
        if (opera != NULL) {
            s32 result = NANDCreate("/tmp/opera.arc", NAND_PERM_RUSR | NAND_PERM_WUSR, 0);

            if (result == NAND_RESULT_OK || result == NAND_RESULT_EXISTS) {
                NANDFileInfo file;

                if (NANDOpen("/tmp/opera.arc", &file, NAND_ACCESS_WRITE) == NAND_RESULT_OK &&
                    NANDWrite(&file, opera, operaSize) > 0) {
                    NANDClose(&file);
                    operaCopied = TRUE;
                }
            }

            MEM1Free(opera);
        }

        if (mInfo->layoutBuf != NULL && mInfo->speakerSeBuf != NULL && mInfo->msgBuf != NULL &&
            mInfo->configBuf != NULL &&
            (mInfo->workBuf != NULL || mInfo->externalWorkBuf != NULL) && mSoundData != NULL &&
            mSoundBuffer != NULL && operaCopied) {
            f32 scale;

            mIsReady = TRUE;
            mInfo->unk14 = 0;
            mInfo->unk18 = 1;
            mInfo->unk20 = 0;
            mInfo->unk34 = 1.3684211f;
            scale = 1.0f;
            mInfo->unk38 = scale;
            if ((VITVMode)gRenderMode.tvInfo == VI_TVMODE_PAL_INT) {
                scale = 1.2f;
            }
            mInfo->unk30 = scale;
            mInfo->unk24 = 0;

            mAllocator1 = allocator1;
            mAllocator2 = allocator2;
            fn_80083A80(allocator1, allocator2);
            fn_80083D04(mInfo);
            fn_80071FD8(mInfo);
            fn_80072530(mSoundData, mSoundBuffer, HOME_BUTTON_SOUND_BUFFER_SIZE);
            fn_80072340(gWidescreen);
        }
    }

    if (mIsReady && manualPage != NULL) {
        mManualContent = manualContent;
        mManualPath = manualPath;
        unk24 = 0;
        mManualPage = manualPage;
        strncpy(mManualPageBuf, manualPage, sizeof(mManualPageBuf) - 1);
        mManualPageBuf[sizeof(mManualPageBuf) - 1] = '\0';
        unkE = FALSE;
        mOpenManual = FALSE;
        mInManual = FALSE;
        mResetRequested = FALSE;
    } else {
        mIsReady = FALSE;
    }

    mIsActive = FALSE;
    mResult = HOME_BUTTON_RESULT_NONE;
}

HomeButton::~HomeButton() {
    if (mIsReady) {
        fn_800725EC();
        fn_80072100();
    }

    if (mSoundBuffer != NULL) {
        MEM2Free(mSoundBuffer);
    }

    if (mSoundData != NULL) {
        MEM2Free(mSoundData);
    }

    if (mInfo != NULL) {
        if (mInfo->workBuf != NULL) {
            MEM2Free(mInfo->workBuf);
        }

        if (mInfo->configBuf != NULL) {
            MEM2Free(mInfo->configBuf);
        }

        if (mInfo->msgBuf != NULL) {
            MEM2Free(mInfo->msgBuf);
        }

        if (mInfo->speakerSeBuf != NULL) {
            MEM2Free(mInfo->speakerSeBuf);
        }

        if (mInfo->layoutBuf != NULL) {
            MEM2Free(mInfo->layoutBuf);
        }

        delete mInfo;
    }
}

void HomeButton::Init() {}

s32 HomeButton::Calc() {
    if (!mIsActive && !mOpenManual) {
        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
            if (gTrig[i] & WPAD_BUTTON_HOME) {
                fn_80072184();
                mIsActive = TRUE;
                break;
            }
        }
    }

    mResult = HOME_BUTTON_RESULT_NONE;

    if (mIsActive) {
        HomeButtonController controllers[WPAD_MAX_CONTROLLERS];
        KPADStatus statuses[WPAD_MAX_CONTROLLERS];
        f32 scaleY = 1.2f * (gWidescreen ? 1.1666666f : 1.0f);
        f32 menuScale = 0.908f * scaleY;
        f32 menuHeight = menuScale * GetScreenHeight();
        f32 scaleX = menuHeight * gRenderMode.fbWidth /
                     (gRenderMode.viWidth * GetScreenWidth());

        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
            s32 devType;
            s32 result = WPADProbe(i, &devType);

            if (gKPADLatest[i] >= 0) {
                statuses[i] = gKPADStatus[i][gKPADLatest[i]];
            } else {
                statuses[i] = gKPADStatus[i][0];
            }

            switch (result) {
            case WPAD_ERR_BUSY: {
                u8 type;
                s32 err;

                err = statuses[i].wpad_err;
                type = statuses[i].dev_type;

                memset(&statuses[i], 0, sizeof(KPADStatus));
                statuses[i].dev_type = type;
                statuses[i].wpad_err = err;
            }
            case WPAD_ERR_OK:
            case WPAD_ERR_TRANSFER:
                controllers[i].status = &statuses[i];
                statuses[i].pos.x *= scaleX;
                statuses[i].pos.y *= scaleY;
                break;
            default:
                controllers[i].status = NULL;
                break;
            }
        }

        if (fn_800721B0(controllers) >= 0) {
            switch (fn_80072320()) {
            case 0:
                break;
            case 1:
                mResult = HOME_BUTTON_RESULT_1;
                break;
            case 2:
                mResult = HOME_BUTTON_RESULT_2;
                break;
            case 3:
                if (mManualContent >= 2) {
                    mOpenManual = TRUE;
                }
                break;
            }

            mIsActive = FALSE;
        } else {
            fn_80072620();
        }
    }

    if (mOpenManual && unkE) {
        fn_8003C054();

        if (unkF) {
            fn_8003D9E0();
        }

        if (!OpenManual()) {
            mResult = HOME_BUTTON_RESULT_3;
        } else {
            if (unkF) {
                fn_8003D6D0();
            }

            if (fn_8003BF08()) {
                mResult = HOME_BUTTON_RESULT_3;
            }

            fn_8003ABAC(lbl_80330BBC, 15);
            VISetBlack(FALSE);
            VIFlush();
            mOpenManual = FALSE;
        }
    }

    mFrameCount++;
    return mResult;
}

void HomeButton::Draw() {
    if (mIsActive) {
        Mtx view;
        Mtx44 proj;

        GXClearVtxDesc();
        GXSetVtxAttrFmt(GX_VTXFMT4, GX_VA_POS, GX_POS_XY, GX_F32, 0);
        GXSetVtxAttrFmt(GX_VTXFMT4, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
        GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
        GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        GXSetNumChans(1);
        GXSetNumTexGens(0);
        GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
        GXSetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_CLEAR);
        GXSetCurrentMtx(GX_PNMTX1);
        GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);

        PSMTXIdentity(view);
        C_MTXOrtho(proj, 228.0f, -228.0f, 0.5f * -GetScreenWidth(), 0.5f * GetScreenWidth(), 0.0f,
                   500.0f);
        GXLoadPosMtxImm(view, GX_PNMTX1);
        GXSetProjection(proj, GX_ORTHOGRAPHIC);

        fn_800721F4();
    }
}

inline void HomeButton::CheckHeaps() {
    fn_80110A04(mAllocator1->heap);
    fn_80110A04(mAllocator2->heap);
    fn_80110A04(gMEM1Heap);
    fn_80110A04(gMEM2Heap);
}

BOOL HomeButton::OpenManual() {
    BOOL success = FALSE;
    u32 size;
    void* manual;

    CheckHeaps();
    CheckHeaps();

    manual = LoadContentFile(mManualContent, mManualPath, 32, &size, mAllocator2->heap);
    if (manual != NULL) {
        s32 chan;

        VISetBlack(TRUE);
        VIFlush();
        VIWaitForRetrace();
        SetVideoMode(gProgressive, gWidescreen, TRUE);
        VISetBlack(FALSE);
        VIFlush();

        CheckHeaps();

        chan = 0;
        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
            if (gKPADLatest[i] >= 0) {
                chan = i;
                break;
            }
        }

        success = fn_80083AD8() == 0;
        if (success) {
            fn_80083C9C(manual);
            CheckHeaps();
            fn_80083B3C(&gRenderMode, &gRenderMode, 0);

            success = fn_80083B44(gWidescreen ? 808 : 608, 456) != 0;
            if (success) {
                fn_80083B48(12);
                CheckHeaps();

                success = fn_80083B50(0x1400000 - size) != 0;
                if (success) {
                    CheckHeaps();
                    fn_80083CA4(mManualPage);

                    if (!mResetRequested) {
                        const char* page;

                        mInManual = TRUE;
                        page = fn_80083CA0(DrawManualFade, mManualPageBuf, chan);
                        mInManual = FALSE;
                        strncpy(mManualPageBuf, page, sizeof(mManualPageBuf) - 1);
                    }

                    CheckHeaps();
                    fn_80083C44();
                }

                CheckHeaps();
                fn_80083B4C();
            }

            CheckHeaps();
            fn_80083ADC();
        }

        CheckHeaps();
        VISetBlack(TRUE);
        VIFlush();
        VIWaitForRetrace();
        VIWaitForRetrace();
        SetVideoMode(gProgressive, gWidescreen, FALSE);
        MEMFreeToAllocator(mAllocator2, manual);
    }

    CheckHeaps();
    CheckHeaps();

    return success;
}

static inline void SetFadeColor(u8 alpha) {
    GXColor fade = {0, 0, 0, 0};
    fade.a = alpha;
    GXSetTevColor(GX_TEVREG1, fade);
}

static void DrawManualFade(u8 alpha, GXRenderModeObj* rmode) {
    if (alpha != 0) {
        SetDefaultGXState();
        SetOrthoProjection();

        GXColor clear = {0, 0, 0, 0};
        GXSetTevColor(GX_TEVREG0, clear);

        SetFadeColor(alpha);

        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(0.0f, 0.0f, 0.0f);
        GXTexCoord2f32(0.0f, 0.0f);
        GXPosition3f32(GetScreenWidth(), 0.0f, 0.0f);
        GXTexCoord2f32(0.0f, 0.0f);
        GXPosition3f32(GetScreenWidth(), 456.0f, 0.0f);
        GXTexCoord2f32(0.0f, 0.0f);
        GXPosition3f32(0.0f, 456.0f, 0.0f);
        GXTexCoord2f32(0.0f, 0.0f);
        GXEnd();
    }

    GXSetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GXSetDispCopyDst(rmode->fbWidth, 456);
    GXSetDispCopyYScale(GXGetYScaleFactor(rmode->efbHeight, rmode->xfbHeight));
    GXCopyDisp(gCurrentXfb, GX_TRUE);
    GXDrawDone();

    VIConfigure(rmode);
    VISetNextFrameBuffer(gCurrentXfb);
    VIFlush();
    VIWaitForRetrace();
}

void HomeButton::OnReset() {
    if (mInManual) {
        fn_80083CAC(0);
    }

    mResetRequested = TRUE;
}
