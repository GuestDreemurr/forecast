#ifndef CHANNEL_SCENE_BASE_H
#define CHANNEL_SCENE_BASE_H
#include <types.h>
#include <revolution/GX.h>
#include <revolution/MTX.h>
#include <nw4r/ut/ut_TextWriterBase.h>

class HomeButton;
class SimpleModel;
class Fade;
class Sound;
class ButtonGroup;

// Base class of the channel's scenes (d_scene.cpp). Runs a small state machine: each state is a
// member function called with mStatePhase = -1 on exit and counting up from 0 while active.
class SceneBase {
public:
    typedef void (SceneBase::*DrawFunc)();
    typedef BOOL (SceneBase::*StateFunc)();

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
    virtual BOOL unk44() {                   // at 0x44
        return TRUE;
    }
    virtual BOOL unk48() {                   // at 0x48
        return TRUE;
    }

    void DrawTimeJP();
    void DrawTimeUS();
    void DrawTimeEN();
    void DrawTimeDE();
    void DrawTimeFR();
    void DrawTimeES();
    void DrawTimeIT();
    void DrawTimeNL();
    void UpdateMenuFade();
    BOOL StateMain();
    BOOL StateReset();
    BOOL StateReturnToMenu();
    BOOL StateFatal();

    void Calc();
    void RequestFatal();

    inline void UpdatePointerOverMenu() const;

    BOOL IsState(StateFunc state) const {
        return mState == state;
    }

    void ChangeState(StateFunc state) {
        if (mState) {
            mStatePhase = -1;
            (this->*mState)();
        }

        mStatePhase = 0;
        mState = state;

        if (mState) {
            (this->*mState)();
        }
    }

    nw4r::ut::TextWriterBase<wchar_t> mTextWriter; // at 0x4
    DrawFunc mDrawFunc;                            // at 0x64, per-language clock
    StateFunc mState;                              // at 0x70
    f32 mClockX;          // at 0x7C
    f32 mClockY;          // at 0x80
    f32 unk84;            // at 0x84
    f32 mMenuBarY;        // at 0x88, pointer above this counts as over the menu bar
    f32 unk8C;            // at 0x8C
    f32 unk90;            // at 0x90
    f32 mAmPmOffsetY;     // at 0x94
    s32 mStatePhase;      // at 0x98
    s32 mClockAlpha;      // at 0x9C
    s32 unkA0;            // at 0xA0
    u8 unkA4;             // at 0xA4
    void* mLayoutArc;     // at 0xA8
};

extern HomeButton* gHomeButton;
extern SimpleModel* gEarthModel;
extern u8 gFatalRequested;
extern u8 gReturnToMenuRequested;
extern s32 gGlobeAlpha;
extern Sound* gSound;
extern Fade* gFade;
extern Fade* gFade2;
extern u8 gPointerOverMenu;
extern u8 gMenuVisible;
extern f32 gMenuBrightness;

u32 GetLanguageTexture();
BOOL LoadSysFont();
void FreeSysFonts();
BOOL LoadEarthModel();
BOOL FreeEarthModel();
void ToDegrees(u16 lon, u16 lat, Vec2* out);
void PlaySE(s32 id);
void UpdateButtons(ButtonGroup* group, s32 hoverSound);
void ClearHoveredButtons();
s32 CheckButtonHeld(const char* name, u32 buttons);
s32 CheckButtonPressed(const char* name, u32 buttons);
f32 CalcDateWidth(const wchar_t* str);
void DrawDateCentered(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
                      const GXColor* shadowColor);
void DrawDate(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
              const GXColor* shadowColor);
f32 CalcNumWidth(const wchar_t* str, f32 spacing);
void DrawNumCentered(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, f32 spacing, const GXColor* color,
                     const GXColor* color2);
void DrawNum(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, f32 spacing, const GXColor* color,
             const GXColor* color2);
void DrawNumRightAligned(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
                         const GXColor* color2);
f32 CalcTempWidth(const wchar_t* str);
f32 GetTempGlyphHeight(const wchar_t* str);
void DrawTempCentered(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
                      const GXColor* shadowColor);
void DrawTemp(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
              const GXColor* shadowColor);

#endif
