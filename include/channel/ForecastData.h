#ifndef CHANNEL_FORECAST_DATA_H
#define CHANNEL_FORECAST_DATA_H
#include <types.h>

// One place entry of forecast.bin (size 0x18)
struct CityInfo {
    u32* mId;        // at 0x0
    wchar_t* mName;  // at 0x4
    u8 unk8[0x18 - 0x8];
};

struct ForecastHeader {
    u8 unk0[0x19];     // at 0x0
    u8 mUnitType;      // at 0x19, 1 = metric, 2 = imperial
    u8 unk1A[0x50 - 0x1A];
    s32 mNumPlaces;    // at 0x50
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
