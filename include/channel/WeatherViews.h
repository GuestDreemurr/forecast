#ifndef CHANNEL_WEATHER_VIEWS_H
#define CHANNEL_WEATHER_VIEWS_H
#include <types.h>
#include <nw4r/snd/snd_SoundHandle.h>
#include <revolution/OS.h>
#include <revolution/MTX.h>
#include <nw4r/ut.h>

class ForecastData;

class ButtonGroup;
class LayoutButton;
class Fade;

// Connect.cpp: downloads forecast.bin/short.bin through WiiConnect24 and shows the "connecting" screen
// (size 0x460)
class Connect {
public:
    Connect(void* arc, ForecastData* data);
    ~Connect();

    void Start();
    void Calc();
    void Draw();
    BOOL IsDone();
    void PauseSound(bool pause);
    void DrawProgress();
    void SetErrorCode(s32 wc24Code, s32 localCode);

    ForecastData* mData;          // at 0x0
    void* mForecastBuf;           // at 0x4
    void* mShortBuf;              // at 0x8
    Fade* mFade;                  // at 0xC
    s32 mState;                   // at 0x10
    s32 mTask;                    // at 0x14
    s64 mForecastTime;            // at 0x18
    s64 mShortTime;               // at 0x20
    u32 mForecastSize;            // at 0x28
    u32 mShortSize;               // at 0x2C
    s32 mTaskState;               // at 0x30
    s32 mTaskResult;              // at 0x34
    s32 mForecastCheck;           // at 0x38
    s32 mShortCheck;              // at 0x3C
    char mForecastUrl[0x200];     // at 0x40
    char mShortUrl[0x200];        // at 0x240
    wchar_t* mServerMessage;      // at 0x440
    s32 mProgressFrame;           // at 0x444
    ButtonGroup* mConnectLayout;  // at 0x448, error1.brlyt
    ButtonGroup* mErrorLayout;    // at 0x44C, error0.brlyt
    s32 mDisplayState;            // at 0x450
    nw4r::snd::SoundHandle mSound; // at 0x454
    u8 mSoundPlaying;             // at 0x458
    u8 unk459[0x460 - 0x459];     // at 0x459
};

class ButtonGroup;
class LayoutButton;
class Fade;

// The dialog shown for connection and save errors (size 0x18)
class ErrorWindow {
public:
    ErrorWindow(void* arc);
    ~ErrorWindow();

    void Open(s32 message);
    void Calc();
    void Draw();

    Fade* mFade;               // at 0x0
    ButtonGroup* mYesNoLayout; // at 0x4, error3.brlyt
    ButtonGroup* mSaveLayout;  // at 0x8, error4.brlyt
    ButtonGroup* mFatalLayout; // at 0xC, error2.brlyt
    s32 mState;                // at 0x10
    s32 mMessage;              // at 0x14
};

void FormatTime(wchar_t* buf, size_t size, s64 time, s32 region, u8 language);

// noerase/savedata.dat (size 0x20)
struct SaveData {
    u32 unk0;          // at 0x0
    u32 mCountryCode;  // at 0x4
    u32 mCityId;       // at 0x8
    s32 mTempUnit;     // at 0xC
    s32 mWindUnit;     // at 0x10
    u8 unk14[0xC];     // at 0x14
};

void SetSaveDataBuffer(SaveData* buf, u32 size);
s32 ReadSaveData();
s32 WriteSaveData(SaveData* data);

struct CityInfo;

// A row of the area/city list (size 0x48)
struct AddressEntry {
    AddressEntry() : mLeft(0.0f), mTop(0.0f), mRight(0.0f), mBottom(0.0f) {}
    ~AddressEntry() {}

    AddressEntry* mPrev;  // at 0x0
    AddressEntry* mNext;  // at 0x4
    CityInfo* mPlace;     // at 0x8, NULL for "other regions"
    wchar_t* mName;       // at 0xC
    const wchar_t* mLabel; // at 0x10
    f32 mX;               // at 0x14
    f32 mY;               // at 0x18
    f32 mWidth;           // at 0x1C
    f32 mHeight;          // at 0x20
    f32 mNameScaleX;      // at 0x24
    f32 mNameScaleY;      // at 0x28
    f32 mLabelScaleX;     // at 0x2C
    f32 mLabelScaleY;     // at 0x30
    f32 mLeft;            // at 0x34
    f32 mTop;             // at 0x38
    f32 mRight;           // at 0x3C
    f32 mBottom;          // at 0x40
    u8 mHovered;          // at 0x44
    u8 mWasHovered;       // at 0x45
    u8 mHidden;           // at 0x46
};

// d_weather_address.cpp: area/city picker (size 0x160)
class WeatherAddress {
public:
    typedef BOOL (WeatherAddress::*StateFunc)(s32 arg);
    typedef void (WeatherAddress::*DrawFunc)();
    typedef void (WeatherAddress::*ScrollFunc)();

    WeatherAddress(void* arc);
    ~WeatherAddress();

    void Reset();
    void Draw();
    void Calc();
    void SelectCurrentCity();
    void UpdateInput();
    s32 BuildCityList();
    s32 BuildAreaList();
    void DrawCityList();
    void DrawAreaList();
    void DrawConfirm();
    void ScrollNormal();
    void ScrollDrag();
    void ChangeState(StateFunc state, s32 arg);
    BOOL UpdateCityList();
    BOOL UpdateAreaList();
    BOOL StateCity(s32 arg);
    BOOL StateArea(s32 arg);
    BOOL StateConfirm(s32 arg);
    BOOL StateClose(s32 arg);
    s32 HitTest(AddressEntry* entry);

    AddressEntry* mEntries;       // at 0x0
    AddressEntry* mSelected;      // at 0x4
    AddressEntry* mListHead;      // at 0x8
    ButtonGroup* mActiveLayout;   // at 0xC
    ButtonGroup* mBaseLayout;     // at 0x10, base.brlyt
    ButtonGroup* mListLayout;     // at 0x14, set_area1.brlyt
    ButtonGroup* mConfirmLayout;  // at 0x18, set_area2.brlyt
    LayoutButton* mUpButton;      // at 0x1C
    LayoutButton* mDownButton;    // at 0x20
    LayoutButton* mBackButton;    // at 0x24
    LayoutButton* mTitle;         // at 0x28
    StateFunc mState;             // at 0x2C
    DrawFunc mDrawFunc;           // at 0x38
    ScrollFunc mScrollState;      // at 0x44
    nw4r::ut::TextWriterBase<wchar_t> mWriter; // at 0x50
    AddressEntry mOtherEntry;     // at 0xB0
    f32 mListX;                   // at 0xF8
    f32 mScroll;                  // at 0xFC
    Vec mUpArrowPos;              // at 0x100
    Vec mDownArrowPos;            // at 0x10C
    f32 mListTop;                 // at 0x118
    f32 mRowHeight;               // at 0x11C
    f32 mClipTop;                 // at 0x120
    f32 mClipBottom;              // at 0x124
    f32 mScrollTarget;            // at 0x128
    f32 mScrollMax;               // at 0x12C
    f32 mScrollMin;               // at 0x130
    f32 mScrollSpeed;             // at 0x134
    u32 mAreaId;                  // at 0x138
    s32 mNumPlaces;               // at 0x13C
    s32 mNumEntries;              // at 0x140
    s32 mNumPages;                // at 0x144
    s32 mTopIndex;                // at 0x148
    s32 mPhase;                   // at 0x14C
    s32 mScrollPhase;             // at 0x150
    s32 mTimer;                   // at 0x154
    u8 mUp;                       // at 0x158
    u8 mDown;                     // at 0x159
    u8 mDragging;                 // at 0x15A
    u8 mYes;                      // at 0x15B
    u8 mNo;                       // at 0x15C
    u8 mCanGoBack;                // at 0x15D
    u8 mShowArrows;               // at 0x15E
};

// d_weather_around.cpp: forecast of the surrounding cities on the globe (size 0x310)
class WeatherAround {
public:
    typedef void (WeatherAround::*DrawFunc)();

    WeatherAround(void* arc);
    virtual ~WeatherAround();

    void Reset();
    void Calc();
    void Draw();
    void Show();
    void Open();
    void DrawGlobe();

    u8 unk4[0xA4 - 0x4];   // at 0x4
    DrawFunc mDrawFunc;    // at 0xA4
    u8 unkB0[0x24F - 0xB0];
    u8 mActive;            // at 0x24F
    u8 unk250;             // at 0x250
    u8 unk251[0x310 - 0x251];
};

// d_weather_normal.cpp: the main forecast display (size 0x964)
class WeatherNormal {
public:
    typedef void (WeatherNormal::*DrawFunc)();
    typedef BOOL (WeatherNormal::*StateFunc)(s32 arg);

    WeatherNormal(void* arc);
    ~WeatherNormal();

    void Reset();
    void Calc();
    void Draw();
    void SetCity(s32 arg);
    void Open();
    void Show();
    BOOL ChangeState(StateFunc state, s32 arg);
    void DrawCity();
    BOOL StateOpenAround(s32 arg);
    BOOL StateCloseAround(s32 arg);

    u8 unk0[0x15C];        // at 0x0
    DrawFunc mDrawFunc;    // at 0x15C
    u8 unk168[0x7F9 - 0x168];
    u8 mActive;            // at 0x7F9
    u8 unk7FA[0x964 - 0x7FA];
};

#endif
