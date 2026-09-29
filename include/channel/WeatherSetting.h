#ifndef CHANNEL_WEATHER_SETTING_H
#define CHANNEL_WEATHER_SETTING_H
#include <types.h>
#include <nw4r/ut/ut_TextWriterBase.h>

class ButtonGroup;
class LayoutButton;

// Settings screen (d_weather_setting.cpp): temperature unit, wind unit and the selected city
class WeatherSetting {
public:
    typedef void (WeatherSetting::*StateFunc)(s32 arg);

    WeatherSetting(void* arc);
    ~WeatherSetting();

    void Reset();
    void Draw();
    void Calc();
    void Open();
    void Close();

    void StateMain(s32 arg);
    void StateIdle(s32 arg);
    void UpdateCityName();

    void ChangeState(StateFunc state) {
        if (mState) {
            mStatePhase = -1;
            (this->*mState)(0);
        }

        mStatePhase = 0;
        mState = state;

        if (mState) {
            (this->*mState)(0);
        }
    }

    ButtonGroup* mSetLayout;                         // at 0x0
    ButtonGroup* mBaseLayout;                        // at 0x4
    LayoutButton* mKionSet;                          // at 0x8
    LayoutButton* mCitySet;                          // at 0xC
    LayoutButton* mWindSet;                          // at 0x10
    LayoutButton* mKionBtn;                          // at 0x14
    LayoutButton* mCityBtn;                          // at 0x18
    LayoutButton* mWindBtn;                          // at 0x1C
    void* mCity;                                     // at 0x20
    nw4r::ut::TextWriterBase<wchar_t> mTextWriter;   // at 0x24
    StateFunc mState;                                // at 0x84
    f32 mTextX;                                      // at 0x90
    f32 mTextY;                                      // at 0x94
    f32 mTextMaxWidth;                               // at 0x98
    f32 mTextHeight;                                 // at 0x9C
    f32 mTextScaleX;                                 // at 0xA0
    f32 mTextScaleY;                                 // at 0xA4
    f32 mTextCharSpace;                              // at 0xA8
    s32 mKionBrightness;                             // at 0xAC
    s32 mCityBrightness;                             // at 0xB0
    s32 mWindBrightness;                             // at 0xB4
    s32 mStatePhase;                                 // at 0xB8
    s32 unkBC;                                       // at 0xBC
    wchar_t mCityName[64];                           // at 0xC0
};

#endif
