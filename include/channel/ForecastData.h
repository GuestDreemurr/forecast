#ifndef CHANNEL_FORECAST_DATA_H
#define CHANNEL_FORECAST_DATA_H
#include <types.h>
#include <revolution/OS.h>

struct WeatherInfo;
struct PlaceEntry;
struct ShortEntry;

// One place entry of forecast.bin (size 0x18)
struct CityInfo {
    CityInfo();
    ~CityInfo();
    void Setup(void* base, PlaceEntry* entry, u32 index);

    u32 GetId() const {
        return *mId;
    }

    u32* mId;          // at 0x0, points at the PlaceEntry
    wchar_t* mName;    // at 0x4
    u8 unk8[0xC - 0x8]; // at 0x8
    wchar_t* mCountry; // at 0xC
    wchar_t* mRegion;  // at 0x10
    s32 mIndex;        // at 0x14
};

// forecast.bin header (size 0x58)
// credits: https://github.com/RiiConnect24/Kaitai-Files/blob/master/Kaitais/forecast_file.ksy
struct ForecastHeader {
    u32 mVersion;             // at 0x0
    u32 mSize;                // at 0x4
    u32 mCRC;                 // at 0x8, over everything after this field
    u32 mOpenTime;            // at 0xC, minutes since 2000
    u32 mTime;                // at 0x10, closing time, minutes since 2000
    u8 mCountry;              // at 0x14
    u8 unk15[0x18 - 0x15];    // at 0x15
    u8 mLanguage;             // at 0x18
    u8 mUnitType;             // at 0x19, 0 = Japan, 1 = Fahrenheit, 2 = Celsius
    u8 unk1A;                 // at 0x1A
    u8 mPadding;              // at 0x1B
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
// credits: https://github.com/RiiConnect24/Kaitai-Files/blob/master/Kaitais/forecast_file_short.ksy
struct ShortHeader {
    u32 mVersion;          // at 0x0
    u32 mSize;             // at 0x4
    u32 mCRC;              // at 0x8
    u8 mOpenTime;          // at 0xC
    u32 mTime;             // at 0x10
    u32 mCountry;          // at 0x14
    u8 mLanguage;          // at 0x18
    u8 unk19[0x1C - 0x19]; // at 0x19
    u32 mNumEntries;       // at 0x1C
    u32 mEntryOffset;      // at 0x20
};

// One day of a forecast entry (size 0x1C)
struct DayForecast {
    u16 mWeather;          // at 0x0
    u16 mWeatherParts[4];  // at 0x2
    s8 mMaxC;              // at 0xA, -128 = none
    s8 mMaxDiffC;          // at 0xB, change from the day before
    s8 mMinC;              // at 0xC
    s8 mMinDiffC;          // at 0xD
    s8 mMaxF;              // at 0xE
    s8 mMaxDiffF;          // at 0xF
    s8 mMinF;              // at 0x10
    s8 mMinDiffF;          // at 0x11
    u8 mPercent[4];        // at 0x12, precipitation chances, 0xFF = none
    u8 mWindDirection;     // at 0x16
    u8 mWindSpeedKmh;      // at 0x17
    u8 mWindSpeedMph;      // at 0x18
    u8 mUVIndex;           // at 0x19
    u8 mLaundryIndex;      // at 0x1A
    u8 mPollenIndex;       // at 0x1B
};

// One day of the week-long outlook (size 0x8)
struct WeekForecast {
    u16 mWeather;          // at 0x0
    s8 mMaxC;              // at 0x2
    s8 mMinC;              // at 0x3
    s8 mMaxF;              // at 0x4
    s8 mMinF;              // at 0x5
    u8 mPercent;           // at 0x6
    u8 unk7;               // at 0x7
};

// Long-range forecast of a city (size 0x80)
struct ForecastEntry {
    u32 mId;               // at 0x0
    u32 mTime;             // at 0x4, minutes since 2000
    u32 mOffset;           // at 0x8, subtracted from mTime
    u8 unkC;               // at 0xC
    u8 unkD[0x10 - 0xD];   // at 0xD
    DayForecast mDays[2];  // at 0x10
    WeekForecast mWeek[7]; // at 0x48
};

// Today/tomorrow summary of a city (size 0x48)
struct SummaryEntry {
    u32 mId;               // at 0x0
    u32 mTime;             // at 0x4
    u8 unk8[0xC - 0x8];    // at 0x8
    u8 unkC;               // at 0xC
    u8 unkD[0x10 - 0xD];   // at 0xD
    DayForecast mDays[2];  // at 0x10
};

// Weather icon code and its text (size 0x8)
struct WeatherType {
    u16 mCode;             // at 0x0
    u16 mIcon;             // at 0x2, 0x8000 = night variant
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
    u16 mLongitude;        // at 0x10
    u16 mLatitude;         // at 0x12
    u8 unk14;              // at 0x14
    u8 unk15;              // at 0x15
    u8 unk16[2];           // at 0x16
};

// Current weather of a city from short.bin (size 0x18)
struct ShortEntry {
    u32 mId;               // at 0x0
    u32 mTime;             // at 0x4
    u8 unk8[0xC - 0x8];    // at 0x8
    u16 mWeather;          // at 0xC
    u8 unkE;               // at 0xE
    s8 mTempC;             // at 0xF
    s8 mTempF;             // at 0x10
    u8 mWindDirection;     // at 0x11
    u8 mWindSpeedKmh;      // at 0x12
    u8 mWindSpeedMph;      // at 0x13
    u8 unk14[0x18 - 0x14]; // at 0x14
};

// A place with its long-range, summary and current-weather entries (size 0x14)
// The forecast of a city for the current time
struct CityForecast {
    CityForecast();
    ~CityForecast();
    void Setup(void* base, ForecastEntry* entry);

    ForecastEntry* mEntry; // at 0x0
    OSCalendarTime mTime;  // at 0x4
    u32 mMinutes;          // at 0x2C
};

// The today/tomorrow summary of a city
struct CitySummary {
    CitySummary();
    ~CitySummary();
    void Setup(void* base, SummaryEntry* entry);

    SummaryEntry* mEntry; // at 0x0
};

// The current weather of a city
struct CityNow {
    CityNow();
    ~CityNow();
    void Setup(void* base, ShortEntry* entry);

    ShortEntry* mEntry; // at 0x0
};

// A UV/laundry/pollen index and its description
struct IndexInfo {
    IndexText* mIndex;    // at 0x0
    const wchar_t* mText; // at 0x4
};

struct UVIndexInfo : public IndexInfo {
    UVIndexInfo() {
        mIndex = NULL;
        mText = NULL;
    }
    ~UVIndexInfo() {}
    s32 Setup(void* base, IndexText* entry);
};

struct LaundryIndexInfo : public IndexInfo {
    LaundryIndexInfo() {
        mIndex = NULL;
        mText = NULL;
    }
    ~LaundryIndexInfo() {}
    s32 Setup(void* base, IndexText* entry);
};

struct PollenIndexInfo : public IndexInfo {
    PollenIndexInfo() {
        mIndex = NULL;
        mText = NULL;
    }
    ~PollenIndexInfo() {}
    s32 Setup(void* base, IndexText* entry);
};

class City {
public:
    City(CityInfo* info);
    ~City();

    CityInfo* GetInfo() const { return mInfo; }
    CityForecast* GetForecast() const { return mForecast; }
    CitySummary* GetSummary() const { return mSummary; }

    CityInfo* mInfo;   // at 0x0
    CityForecast* mForecast; // at 0x4
    CitySummary* mSummary; // at 0x8
    CityNow* mNow;     // at 0xC
    u8 mIsNight;       // at 0x10
    bool mIsDay;        // at 0x11
};

// forecast.bin and short.bin (size 0x54)
class ForecastData {
public:
    ForecastData();
    ~ForecastData();

    s32 LoadForecast(void* data);
    s32 LoadShort(void* data);
    CityForecast* FindForecast(const u32& id);
    CitySummary* FindSummary(const u32& id);
    CityNow* FindNow(const u32& id);
    WeatherInfo* FindWeatherInfo(const u32& code);
    u16 GetWeatherIcon(const u32& code);
    IndexInfo* FindUVIndex(const u8& code);
    IndexInfo* FindLaundryIndex(const u8& code);
    IndexInfo* FindPollenIndex(const u8& code);

    void* mForecastBin;                  // at 0x0
    void* mShortBin;                     // at 0x4
    CityForecast* mForecasts;            // at 0x8
    CitySummary* mSummaries;             // at 0xC
    WeatherInfo* mWeatherInfo;           // at 0x10, one per weather type
    UVIndexInfo* mUVIndices;             // at 0x14
    LaundryIndexInfo* mLaundryIndices;   // at 0x18
    PollenIndexInfo* mPollenIndices;     // at 0x1C
    CityInfo* mPlaces;                   // at 0x20
    CityNow* mNow;                       // at 0x24
    ForecastHeader* mHeader;             // at 0x28
    ShortHeader* mShortHeader;           // at 0x2C
    ForecastEntry* mForecastEntries;     // at 0x30
    SummaryEntry* mSummaryEntries;       // at 0x34
    WeatherType* mWeatherTypes;          // at 0x38
    IndexText* mUVTexts;                 // at 0x3C
    IndexText* mLaundryTexts;            // at 0x40
    IndexText* mPollenTexts;             // at 0x44
    PlaceEntry* mPlaceEntries;           // at 0x48
    ShortEntry* mShortEntries;           // at 0x4C
    const wchar_t* mMessage;             // at 0x50
};

extern ForecastData* gForecastData;
extern City** gCities;
extern City* gCurrentCity;

City* FindCity(u32 id);
BOOL CheckForecastData(void* forecast, u32 forecastSize, s32* forecastResult, void* shortData, u32 shortSize,
                       s32* shortResult);

#endif
