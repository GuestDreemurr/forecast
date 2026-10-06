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
    CityInfo* self = this;
    u32 name = entry->mNameOffset;
    CityInfo** pself; // HACK: address of a local copy of this changes the schedule
    u32 country = entry->mCountryOffset;
    self->mIndex = index;
    self->mId = &entry->mId;
    self->mName = (wchar_t*)((u8*)base + name);
    pself = &self;
    if (country != 0) {
        (*pself)->mCountry = (wchar_t*)((u8*)base + country);
    }
    if (((PlaceEntry*)(*pself)->mId)->mRegionOffset != 0) {
        self->mRegion = (wchar_t*)((u8*)base + ((PlaceEntry*)self->mId)->mRegionOffset);
    }
}

#pragma scheduling off
s32 PollenIndexInfo::Setup(void* base, IndexText* entry) {
    u32 offset = entry->mTextOffset;
    mIndex = entry;
    if (offset == 0) {
        return 0x17;
    }
    mText = (const wchar_t*)((u8*)base + offset);
    return 0x18;
}
#pragma scheduling reset
