// The current weather, place and pollen entries of forecast.bin/short.bin
#include <channel/ForecastData.h>

CityNow::CityNow() {
    mEntry = NULL;
}

CityNow::~CityNow() {
    mEntry = NULL;
}

void CityNow::Setup(void* base, ShortEntry* entry) {
    mEntry = entry;
}

CityInfo::CityInfo() {
    mId = NULL;
    mName = NULL;
    *(u32*)unk8 = 0;
    mCountry = NULL;
    mRegion = NULL;
}

CityInfo::~CityInfo() {}
void CityInfo::Setup(void* base, PlaceEntry* entry, u32 index) {
    u32 name = entry->mNameOffset;
    u32 country = entry->mCountryOffset;
    mIndex = index;
    mId = &entry->mId;
    mName = (wchar_t*)((u8*)base + name);
    if (country != 0) {
        mCountry = (wchar_t*)((u8*)base + country);
    }
    if (((PlaceEntry*)mId)->mRegionOffset != 0) {
        mRegion = (wchar_t*)((u8*)base + ((PlaceEntry*)mId)->mRegionOffset);
    }
}
s32 PollenIndexInfo::Setup(void* base, IndexText* entry) {
    u32 offset = entry->mTextOffset;
    mIndex = entry;
    if (offset == 0) {
        return 0x17;
    }
    mText = (const wchar_t*)((u8*)base + offset);
    return 0x18;
}
