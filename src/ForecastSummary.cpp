// The summary, weather type and UV index entries of forecast.bin
#include <channel/ForecastData.h>
#include <channel/WeatherBase.h>

CitySummary::CitySummary() {
    mEntry = NULL;
}

CitySummary::~CitySummary() {}

void CitySummary::Setup(void* base, SummaryEntry* entry) {
    mEntry = entry;
}

WeatherInfo::WeatherInfo() {
    mType = NULL;
    mText = NULL;
}

WeatherInfo::~WeatherInfo() {}

s32 WeatherInfo::Setup(void* base, WeatherType* type) {
    u32 offset = type->mTextOffset;
    mType = type;
    if (offset == 0) {
        return 0x13;
    }
    mText = (const wchar_t*)((u8*)base + offset);
    return 0x18;
}

s32 UVIndexInfo::Setup(void* base, IndexText* entry) {
    u32 offset = entry->mTextOffset;
    mIndex = entry;
    if (offset == 0) {
        return 0x15;
    }
    mText = (const wchar_t*)((u8*)base + offset);
    return 0x18;
}
