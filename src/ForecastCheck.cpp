// Validation of downloaded forecast.bin and short.bin
#include <channel/ForecastData.h>
#include <channel/System.h>

#include <revolution/NET.h>
#include <revolution/OS.h>

u32 GetCurrentMinutes();
s32 CheckDayForecast(ForecastHeader* header, u32 id, s32 day, DayForecast* forecast);
s32 CheckWeekForecast(ForecastHeader* header, u32 id, s32 day, WeekForecast* forecast);
void MinutesToCalendarTime(u32 minutes, OSCalendarTime* time);

// Points dst at a table inside the file; tables must be 4-byte aligned
#define GET_TABLE(dst, type, base, offset, err)                                                              \
    {                                                                                                        \
        type* p = (type*)((u8*)(base) + (offset));                                                           \
        if (((u32)p & 3) == 0) {                                                                             \
            dst = p;                                                                                         \
        }                                                                                                    \
        if ((s32)p & 3) {                                                                                   \
            err = -1;                                                                                        \
        }                                                                                                    \
    }

BOOL CheckForecastData(void* forecast, u32 forecastSize, s32* forecastResult, void* shortData, u32 shortSize,
                       s32* shortResult) {
    // TODO: these are probably real locals, but that changes register allocation
#define header ((ForecastHeader*)forecast)
#define shortHeader ((ShortHeader*)shortData)
    ForecastEntry* forecasts;
    SummaryEntry* summaries;
    WeatherType* weatherTypes;
    IndexText* uvIndices;
    IndexText* laundryIndices;
    IndexText* pollenIndices;
    PlaceEntry* places;
    ShortEntry* shorts;
    s32 err = 0;
    s32 shortErr = 0;
    u32 now = GetCurrentMinutes();
    OSCalendarTime time;

    if (header != NULL) {
        if (((u32)header & 3) == 0) {
            goto headerAligned;
        }

        *forecastResult = -1;
        *shortResult = 0;
        return FALSE;

    headerAligned:

        forecasts = (ForecastEntry*)((u8*)header + header->mForecastOffset);
        if (((u32)forecasts & 3) == 0) {
            forecasts = (ForecastEntry*)((u8*)header + header->mForecastOffset);
        }
        if ((s32)forecasts & 3) {
            err = -1;
        }
        GET_TABLE(summaries, SummaryEntry, header, header->mSummaryOffset, err);
        GET_TABLE(weatherTypes, WeatherType, header, header->mWeatherTypeOffset, err);
        GET_TABLE(uvIndices, IndexText, header, header->mUVIndexOffset, err);
        GET_TABLE(laundryIndices, IndexText, header, header->mLaundryIndexOffset, err);
        GET_TABLE(pollenIndices, IndexText, header, header->mPollenIndexOffset, err);
        PlaceEntry* p = (PlaceEntry*)((u8*)header + header->mPlaceOffset);
        if (((u32)p & 3) == 0) {
            places = p;
        } else {
            err = -1;
        }
    } else {
        err = -1;
    }

    if (shortHeader != NULL) {
        if (((u32)shortHeader & 3) == 0) {
            goto shortAligned;
        }

        *forecastResult = err;
        *shortResult = -1;
        return FALSE;

    shortAligned:

        GET_TABLE(shorts, ShortEntry, shortHeader, shortHeader->mEntryOffset, shortErr);
    }

    if (err != 0 || shortErr != 0) {
        *forecastResult = err;
        *shortResult = shortErr;
        return FALSE;
    }

    if ((header->mVersion & 0xFFFF0000) && err == 0) {
        err = -3;
    }

    if (header->mSize != forecastSize) {
        err = -1;
    }

    if (header->mCRC != NETCalcCRC32((u8*)header + 0xC, forecastSize - 0xC)) {
        err = -1;
    }

    if (err == -1) {
        *forecastResult = err;
        *shortResult = shortErr;
        return FALSE;
    }

    MinutesToCalendarTime(header->mTime, &time);
    if (header->mTime < now && err == 0) {
        err = -2;
    }

    if (header->mLanguage != gLanguage) {
        err = -1;
    }

    if (header->mMessageOffset >= header->mSize) {
        err = -1;
    }

    if (header->mMessageOffset & 1) {
        err = -1;
    }

    if (header->mMessageOffset != 0 && err == 0) {
        *forecastResult = err;
        *shortResult = 0;
        return TRUE;
    }

    if (header->mUnitType > 2) {
        err = -1;
    }

    if (header->unk1A > 1) {
        err = -1;
    }

    if (header->mForecastOffset + header->mNumForecasts * sizeof(ForecastEntry) > header->mSize) {
        err = -1;
    }

    if (header->mSummaryOffset + header->mNumSummaries * sizeof(SummaryEntry) > header->mSize) {
        err = -1;
    }

    if (header->mWeatherTypeOffset + header->mNumWeatherTypes * sizeof(WeatherType) > header->mSize) {
        err = -1;
    }

    if (header->mUVIndexOffset + header->mNumUVIndices * sizeof(IndexText) > header->mSize) {
        err = -1;
    }

    if (header->mLaundryIndexOffset + header->mNumLaundryIndices * sizeof(IndexText) > header->mSize) {
        err = -1;
    }

    if (header->mPollenIndexOffset + header->mNumPollenIndices * sizeof(IndexText) > header->mSize) {
        err = -1;
    }

    if (header->mPlaceOffset + header->mNumPlaces * sizeof(PlaceEntry) > header->mSize) {
        err = -1;
    }

    if (err != 0) {
        *forecastResult = err;
        *shortResult = shortErr;
        return FALSE;
    }

    for (s32 i = 0; i < header->mNumForecasts; i++) {
        s32 count = 0;
        for (u32 j = 0; j < header->mNumPlaces; j++) {
            if (forecasts[i].mId == places[j].mId) {
                count++;
            }
        }

        if (count != 1) {
            err = -1;
        }

        if (forecasts[i].unkC > 5 && forecasts[i].unkC != 0xFF) {
            err = -1;
        }

        for (s32 k = 0; k < 2; k++) {
            s32 ret = CheckDayForecast(header, forecasts[i].mId, k, &forecasts[i].mDays[k]);
            if (err == 0) {
                err = ret;
            }
        }

        for (s32 k = 0; k < 7; k++) {
            s32 ret = CheckWeekForecast(header, forecasts[i].mId, k, &forecasts[i].mWeek[k]);
            if (err == 0) {
                err = ret;
            }
        }
    }

    for (s32 i = 0; i < header->mNumSummaries; i++) {
        s32 count = 0;
        for (u32 j = 0; j < header->mNumPlaces; j++) {
            if (summaries[i].mId == places[j].mId) {
                count++;
            }
        }

        if (count != 1) {
            err = -1;
        }

        if (summaries[i].unkC > 5 && summaries[i].unkC != 0xFF) {
            err = -1;
        }

        for (s32 k = 0; k < 2; k++) {
            s32 ret = CheckDayForecast(header, summaries[i].mId, k, &summaries[i].mDays[k]);
            if (err == 0) {
                err = ret;
            }
        }
    }

    for (s32 i = 0; i < header->mNumWeatherTypes; i++) {
        BOOL found = TRUE;
        for (s32 j = 0; j < header->mNumWeatherTypes; j++) {
            if (i != j && weatherTypes[i].mCode == weatherTypes[j].mCode) {
                found = FALSE;
                break;
            }
        }

        if (!found) {
            err = -1;
        }

        if (weatherTypes[i].mTextOffset >= header->mSize) {
            err = -1;
        }

        if (weatherTypes[i].mTextOffset & 1) {
            err = -1;
        }
    }

    for (s32 i = 0; i < header->mNumUVIndices; i++) {
        BOOL found = TRUE;
        for (s32 j = 0; j < header->mNumUVIndices; j++) {
            if (i != j && uvIndices[i].mCode == uvIndices[j].mCode) {
                found = FALSE;
                break;
            }
        }

        if (!found) {
            err = -1;
        }

        if (uvIndices[i].mTextOffset >= header->mSize) {
            err = -1;
        }

        if (uvIndices[i].mTextOffset & 1) {
            err = -1;
        }
    }

    for (s32 i = 0; i < header->mNumLaundryIndices; i++) {
        BOOL found = TRUE;
        for (s32 j = 0; j < header->mNumLaundryIndices; j++) {
            if (i != j && laundryIndices[i].mCode == laundryIndices[j].mCode) {
                found = FALSE;
                break;
            }
        }

        if (!found) {
            err = -1;
        }

        if (laundryIndices[i].mTextOffset >= header->mSize) {
            err = -1;
        }

        if (laundryIndices[i].mTextOffset & 1) {
            err = -1;
        }
    }

    for (s32 i = 0; i < header->mNumPollenIndices; i++) {
        BOOL found = TRUE;
        for (s32 j = 0; j < header->mNumPollenIndices; j++) {
            if (i != j && pollenIndices[i].mCode == pollenIndices[j].mCode) {
                found = FALSE;
                break;
            }
        }

        if (!found) {
            err = -1;
        }

        if (pollenIndices[i].mTextOffset >= header->mSize) {
            err = -1;
        }

        if (pollenIndices[i].mTextOffset & 1) {
            err = -1;
        }
    }

    for (s32 i = 0; i < header->mNumPlaces; i++) {
        BOOL found = TRUE;
        for (s32 j = 0; j < header->mNumPlaces; j++) {
            if (i != j && places[i].mId == places[j].mId) {
                found = FALSE;
                break;
            }
        }

        if (!found) {
            err = -1;
        }

        found = FALSE;
        for (u32 j = 0; j < header->mNumForecasts; j++) {
            if (places[i].mId == forecasts[j].mId) {
                found = TRUE;
                break;
            }
        }

        if (!found) {
            for (u32 j = 0; j < header->mNumSummaries; j++) {
                if (places[i].mId == summaries[j].mId) {
                    found = TRUE;
                    break;
                }
            }
        }

        if (!found) {
            for (u32 j = 0; j < shortHeader->mNumEntries; j++) {
                if (places[i].mId == shorts[j].mId) {
                    found = TRUE;
                    break;
                }
            }
        }

        if (!found) {
            err = -1;
        }

        if (places[i].mNameOffset >= header->mSize) {
            err = -1;
        }

        if (places[i].mNameOffset & 1) {
            err = -1;
        }

        if (places[i].mRegionOffset >= header->mSize) {
            err = -1;
        }

        if (places[i].mRegionOffset & 1) {
            err = -1;
        }

        if (places[i].mCountryOffset >= header->mSize) {
            err = -1;
        }

        if (places[i].mCountryOffset & 1) {
            err = -1;
        }

        if (places[i].unk14 > 9) {
            err = -1;
        }

        if (places[i].unk15 > 3) {
            err = -1;
        }
    }

    if (shortHeader->mVersion & 0xFFFF0000) {
        shortErr = -3;
    }

    if (shortHeader->mSize != shortSize) {
        shortErr = -1;
    }

    if (shortHeader->mCRC != NETCalcCRC32((u8*)shortHeader + 0xC, shortSize - 0xC)) {
        shortErr = -1;
    }

    if (shortHeader->mTime < now && shortErr == 0) {
        shortErr = -2;
    }

    if (shortHeader->mLanguage != gLanguage) {
        shortErr = -1;
    }

    if (shortHeader->mEntryOffset + shortHeader->mNumEntries * sizeof(ShortEntry) > shortHeader->mSize) {
        shortErr = -1;
    }

    if (shortErr != 0) {
        *forecastResult = err;
        *shortResult = shortErr;
        return FALSE;
    }

    for (s32 i = 0; i < *(u32*)&shortHeader->mNumEntries; i++) {
        s32 count = 0;
        for (u32 j = 0; j < header->mNumPlaces; j++) {
            if (shorts[i].mId == places[j].mId) {
                count++;
            }
        }

        if (count != 1) {
            shortErr = -1;
        }

        if (shorts[i].mWeather != 0xFFFF) {
            BOOL found = FALSE;
            for (u32 j = 0; j < header->mNumWeatherTypes; j++) {
                if (*(u16*)&shorts[i].mWeather == weatherTypes[j].mCode) {
                    found = TRUE;
                    break;
                }
            }

            if (!found) {
                shortErr = -1;
            }
        }

        if (shorts[i].unkE > 99 && shorts[i].unkE != 0xFF) {
            shortErr = -1;
        }

        if (shorts[i].mWindDirection > 16 && shorts[i].mWindDirection != 0xFF) {
            shortErr = -1;
        }
    }

    *forecastResult = err;
    BOOL ret = FALSE;
    *shortResult = shortErr;
    if (err == 0 && shortErr == 0) {
        ret = TRUE;
    }

    return ret;
#undef header
#undef shortHeader
}

s32 CheckDayForecast(ForecastHeader* header, u32 id, s32 day, DayForecast* forecast) {
    s32 err = 0;
    BOOL found;
    u32 j;
    s32 i;

    if (forecast->mWeather != 0xFFFF) {
        found = FALSE;
        for (j = 0; j < header->mNumWeatherTypes; j++) {
            if (forecast->mWeather == ((WeatherType*)((u8*)header + header->mWeatherTypeOffset))[j].mCode) {
                found = TRUE;
                break;
            }
        }

        if (!found) {
            err = -1;
        }
    }

    for (i = 0; i < 4; i++) {
        if (forecast->mWeatherParts[i] != 0xFFFF) {
            found = FALSE;
            for (j = 0; j < header->mNumWeatherTypes; j++) {
                if (forecast->mWeatherParts[i] == ((WeatherType*)((u8*)header + header->mWeatherTypeOffset))[j].mCode) {
                    found = TRUE;
                    break;
                }
            }

            if (!found) {
                err = -1;
            }
        }
    }

    for (i = 0; i < 4; i++) {
        if (forecast->mPercent[i] > 100 && forecast->mPercent[i] != 0xFF) {
            err = -1;
        }
    }

    if (forecast->mWindDirection > 16 && forecast->mWindDirection != 0xFF) {
        err = -1;
    }

    if (forecast->mUVIndex != 0xFF) {
        found = FALSE;
        for (j = 0; j < header->mNumUVIndices; j++) {
            if (*(u8*)&forecast->mUVIndex == ((IndexText*)((u8*)header + header->mUVIndexOffset))[j].mCode) {
                found = TRUE;
                break;
            }
        }

        if (!found) {
            err = -1;
        }
    }

    if (forecast->mLaundryIndex != 0xFF && gRegion == 0) {
        found = FALSE;
        for (j = 0; j < header->mNumLaundryIndices; j++) {
            if (*(u8*)&forecast->mLaundryIndex == ((IndexText*)((u8*)header + header->mLaundryIndexOffset))[j].mCode) {
                found = TRUE;
                break;
            }
        }

        if (!found) {
            err = -1;
        }
    }

    if (forecast->mPollenIndex != 0xFF && gRegion == 0) {
        found = FALSE;
        for (j = 0; j < header->mNumPollenIndices; j++) {
            if (*(u8*)&forecast->mPollenIndex == ((IndexText*)((u8*)header + header->mPollenIndexOffset))[j].mCode) {
                found = TRUE;
                break;
            }
        }

        if (!found) {
            err = -1;
        }
    }

    return err;
}

s32 CheckWeekForecast(ForecastHeader* header, u32 id, s32 day, WeekForecast* forecast) {
    s32 err = 0;
    BOOL found;
    u32 j;

    if (forecast->mWeather != 0xFFFF) {
        found = FALSE;
        for (j = 0; j < header->mNumWeatherTypes; j++) {
            if (forecast->mWeather == ((WeatherType*)((u8*)header + header->mWeatherTypeOffset))[j].mCode) {
                found = TRUE;
                break;
            }
        }

        if (!found) {
            err = -1;
        }
    }

    if (forecast->mPercent > 100 && forecast->mPercent != 0xFF) {
        err = -1;
    }

    return err;
}

// Minutes since 2000-01-01 00:00 UTC
u32 GetCurrentMinutes() {
    OSCalendarTime time;
    NETGetUniversalCalendar(&time);

    u32 years = time.year - 2000;
    u32 months = time.month + 1;
    s32 days = time.mday;
    s32 hours = time.hour;
    s32 minutes = time.min;

    days += years * 365;
    for (s32 i = 0; i < years; i += 4) {
        days++;
    }

    for (s32 m = 1; m < months; m++) {
        if (m == 4 || m == 6 || m == 9 || m == 11) {
            days += 30;
        } else if (m == 2) {
            if ((years & 3) == 0) {
                days += 29;
            } else {
                days += 28;
            }
        } else {
            days += 31;
        }
    }

    return minutes + hours * 60 + (days - 1) * 1440;
}
