#ifndef CHANNEL_WEATHER_VIEWS_H
#define CHANNEL_WEATHER_VIEWS_H
#include <types.h>
#include <nw4r/snd/snd_SoundHandle.h>
#include <revolution/OS.h>

class ForecastData;

class ButtonGroup;
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

// d_weather_address.cpp: area/city picker (size 0x160)
class WeatherAddress {
public:
    typedef BOOL (WeatherAddress::*StateFunc)(s32 arg);

    WeatherAddress(void* arc);
    ~WeatherAddress();

    void Calc();
    void Reset();
    void Draw();
    void SelectCurrentCity();
    void ChangeState(StateFunc state, s32 arg);
    BOOL StateClose(s32 arg);

    u8 unk0[0x160];
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
