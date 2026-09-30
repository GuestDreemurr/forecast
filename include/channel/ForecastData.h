#ifndef CHANNEL_FORECAST_DATA_H
#define CHANNEL_FORECAST_DATA_H
#include <types.h>

// One place entry of forecast.bin (size 0x18)
struct CityInfo {
    u32* mId;        // at 0x0
    wchar_t* mName;  // at 0x4
    u8 unk8[0x18 - 0x8];
};

// forecast.bin header (size 0x58)
struct ForecastHeader {
    u32 mVersion;             // at 0x0
    u32 mSize;                // at 0x4
    u32 mCRC;                 // at 0x8, over everything after this field
    u8 unkC[0x10 - 0xC];      // at 0xC
    u32 mTime;                // at 0x10, minutes since 2000
    u32 unk14;                // at 0x14
    u8 mLanguage;             // at 0x18
    u8 mUnitType;             // at 0x19, 1 = metric, 2 = imperial
    u8 unk1A;                 // at 0x1A
    u8 unk1B;                 // at 0x1B
    u32 mMessageOffset;       // at 0x1C
    u32 mNumForecasts;        // at 0x20
    u32 mForecastOffset;      // at 0x24
    u32 mNumSummaries;        // at 0x28
    u32 mSummaryOffset;       // at 0x2C
    u32 mNumWeatherTypes;     // at 0x30
    u32 mWeatherTypeOffset;   // at 0x34
    u32 mNumUVIndices;        // at 0x38
    u32 mUVIndexOffset;       // at 0x3C
    u32 mNumLaundryIndices;   // at 0x40
    u32 mLaundryIndexOffset;  // at 0x44
    u32 mNumPollenIndices;    // at 0x48
    u32 mPollenIndexOffset;   // at 0x4C
    u32 mNumPlaces;           // at 0x50
    u32 mPlaceOffset;         // at 0x54
};

// short.bin header
struct ShortHeader {
    u32 mVersion;          // at 0x0
    u32 mSize;             // at 0x4
    u32 mCRC;              // at 0x8
    u8 unkC[0x10 - 0xC];   // at 0xC
    u32 mTime;             // at 0x10
    u32 unk14;             // at 0x14
    u8 mLanguage;          // at 0x18
    u8 unk19[0x1C - 0x19]; // at 0x19
    u32 mNumEntries;       // at 0x1C
    u32 mEntryOffset;      // at 0x20
};

// One day of a forecast entry (size 0x1C)
struct DayForecast {
    u16 mWeather;          // at 0x0
    u16 mWeatherParts[4];  // at 0x2
    u8 unkA[0x12 - 0xA];   // at 0xA
    u8 mPercent[4];        // at 0x12, precipitation chances, 0xFF = none
    u8 mWindDirection;     // at 0x16
    u8 unk17[0x19 - 0x17]; // at 0x17
    u8 mUVIndex;           // at 0x19
    u8 mLaundryIndex;      // at 0x1A
    u8 mPollenIndex;       // at 0x1B
};

// One day of the week-long outlook (size 0x8)
struct WeekForecast {
    u16 mWeather;          // at 0x0
    u8 unk2[0x6 - 0x2];    // at 0x2
    u8 mPercent;           // at 0x6
    u8 unk7;               // at 0x7
};

// Long-range forecast of a city (size 0x80)
struct ForecastEntry {
    u32 mId;               // at 0x0
    u8 unk4[0xC - 0x4];    // at 0x4
    u8 unkC;               // at 0xC
    u8 unkD[0x10 - 0xD];   // at 0xD
    DayForecast mDays[2];  // at 0x10
    WeekForecast mWeek[7]; // at 0x48
};

// Today/tomorrow summary of a city (size 0x48)
struct SummaryEntry {
    u32 mId;               // at 0x0
    u8 unk4[0xC - 0x4];    // at 0x4
    u8 unkC;               // at 0xC
    u8 unkD[0x10 - 0xD];   // at 0xD
    DayForecast mDays[2];  // at 0x10
};

// Weather icon code and its text (size 0x8)
struct WeatherType {
    u16 mCode;             // at 0x0
    u8 unk2[2];            // at 0x2
    u32 mTextOffset;       // at 0x4
};

// UV/laundry/pollen index and its text (size 0x8)
struct IndexText {
    u8 mCode;              // at 0x0
    u8 unk1[3];            // at 0x1
    u32 mTextOffset;       // at 0x4
};

// A place (size 0x18)
struct PlaceEntry {
    u32 mId;               // at 0x0
    u32 mNameOffset;       // at 0x4
    u32 mRegionOffset;     // at 0x8
    u32 mCountryOffset;    // at 0xC
    u8 unk10[0x14 - 0x10]; // at 0x10
    u8 unk14;              // at 0x14
    u8 unk15;              // at 0x15
    u8 unk16[2];           // at 0x16
};

// Current weather of a city from short.bin (size 0x18)
struct ShortEntry {
    u32 mId;               // at 0x0
    u8 unk4[0xC - 0x4];    // at 0x4
    u16 mWeather;          // at 0xC
    u8 unkE;               // at 0xE
    u8 unkF[0x11 - 0xF];   // at 0xF
    u8 mWindDirection;     // at 0x11
    u8 unk12[0x18 - 0x12]; // at 0x12
};

// A place with its long-range, summary and current-weather entries (size 0x14)
class City {
public:
    City(CityInfo* info);
    ~City();

    CityInfo* mInfo;   // at 0x0
    void** mForecast;  // at 0x4
    void** mSummary;   // at 0x8
    void** mNow;       // at 0xC
    u8 mIsNight;       // at 0x10
    u8 mIsDay;         // at 0x11
};

// forecast.bin and short.bin (size 0x54)
class ForecastData {
public:
    ForecastData();
    ~ForecastData();

    s32 LoadForecast(void* data);
    s32 LoadShort(void* data);
    void** FindForecast(const u32& id);

    u8 unk0[0x20];              // at 0x0
    CityInfo* mPlaces;          // at 0x20
    u8 unk24[0x28 - 0x24];      // at 0x24
    ForecastHeader* mHeader;    // at 0x28
    u8 unk2C[0x54 - 0x2C];      // at 0x2C
};

extern ForecastData* gForecastData;
extern City** gCities;
extern City* gCurrentCity;

City* FindCity(u32 id);
BOOL CheckForecastData(void* forecast, u32 forecastSize, s32* forecastResult, void* shortData, u32 shortSize,
                       s32* shortResult);

#endif
