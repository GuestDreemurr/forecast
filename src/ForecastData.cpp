// City, forecast.bin/short.bin loading and weather icon drawing
#include <channel/ForecastData.h>
#include <channel/Color.h>
#include <channel/SceneBase.h>
#include <channel/System.h>
#include <channel/WeatherBase.h>

#include <nw4r/math.h>
#include <revolution/GX.h>
#include <revolution/OS.h>
#include <wstring.h>

extern u32 gStartMinutes;
extern Color gColorWhite;
extern Color gColorDarkGray;
extern wchar_t sTextBuf[0x100];

void MinutesToCalendarTime(u32 minutes, OSCalendarTime* time);

static inline BOOL IsEvening(const OSCalendarTime& time) {
    BOOL result = FALSE;
    if (time.hour >= 18 && time.hour < 24) {
        result = TRUE;
    }
    return result;
}

static inline BOOL IsDaytime(const OSCalendarTime& time) {
    BOOL result = FALSE;
    if (time.hour >= 6 && time.hour < 18) {
        result = TRUE;
    }
    return result;
}

City::City(CityInfo* info) : mInfo(info), mForecast(NULL), mSummary(NULL), mNow(NULL), mIsNight(FALSE), mIsDay(FALSE) {
    OSCalendarTime time;

    mForecast = gForecastData->FindForecast(mInfo->GetId());
    mSummary = gForecastData->FindSummary(mInfo->GetId());
    mNow = gForecastData->FindNow(mInfo->GetId());

    if (mForecast != NULL) {
        MinutesToCalendarTime(mForecast->mEntry->mTime, &time);
        mIsNight = IsEvening(time);
    } else if (mSummary != NULL) {
        MinutesToCalendarTime(mSummary->mEntry->mTime, &time);
        mIsNight = IsEvening(time);
    }

    if (mNow != NULL) {
        MinutesToCalendarTime(mNow->mEntry->mTime, &time);
        mIsDay = !IsDaytime(time);
    }
}

City::~City() {
    mInfo = NULL;
    mForecast = NULL;
    mSummary = NULL;
    mNow = NULL;
}

ForecastData::ForecastData()
    : mForecastBin(NULL), mShortBin(NULL), mForecasts(NULL), mSummaries(NULL), mWeatherInfo(NULL), mUVIndices(NULL),
      mLaundryIndices(NULL), mPollenIndices(NULL), mPlaces(NULL), mHeader(NULL), mShortHeader(NULL),
      mForecastEntries(NULL), mSummaryEntries(NULL), mWeatherTypes(NULL), mUVTexts(NULL), mLaundryTexts(NULL),
      mPollenTexts(NULL), mPlaceEntries(NULL), mShortEntries(NULL), mMessage(NULL) {}

#define DELETE_ARRAY(array)                                                                                  \
    if (array != NULL) {                                                                                     \
        delete[] array;                                                                                      \
        array = NULL;                                                                                        \
    }

ForecastData::~ForecastData() {
    if (mShortBin != NULL) {
        DELETE_ARRAY(mNow);
        mShortBin = NULL;
    }

    if (mForecastBin != NULL) {
        DELETE_ARRAY(mPlaces);
        DELETE_ARRAY(mPollenIndices);
        DELETE_ARRAY(mLaundryIndices);
        DELETE_ARRAY(mUVIndices);
        DELETE_ARRAY(mWeatherInfo);
        DELETE_ARRAY(mSummaries);
        DELETE_ARRAY(mForecasts);
        mForecastBin = NULL;
    }

    mHeader = NULL;
    mShortHeader = NULL;
    mForecastEntries = NULL;
    mSummaryEntries = NULL;
    mWeatherTypes = NULL;
    mUVTexts = NULL;
    mLaundryTexts = NULL;
    mPollenTexts = NULL;
    mPlaceEntries = NULL;
    mShortEntries = NULL;
}

s32 ForecastData::LoadForecast(void* data) {
    s32 result = 0;
    u32 i;

    mForecastBin = data;
    if (data == NULL) {
        return result;
    }

    ForecastHeader* header = (ForecastHeader*)data;
    mHeader = header;
    if (header->mMessageOffset != 0) {
        mMessage = (const wchar_t*)((u8*)data + header->mMessageOffset);
        return 10;
    }

    if (header->mNumForecasts == 0 && header->mNumSummaries == 0) {
        result = 11;
        goto fail;
    }
    if (header->mNumForecasts != 0 && header->mForecastOffset == 0) {
        result = 15;
        goto fail;
    }
    if (header->mNumSummaries != 0 && header->mSummaryOffset == 0) {
        result = 16;
        goto fail;
    }
    if (header->mNumWeatherTypes == 0) {
        result = 12;
        goto fail;
    }
    if (header->mWeatherTypeOffset == 0) {
        result = 17;
        goto fail;
    }
    if (header->mNumPlaces == 0) {
        result = 13;
        goto fail;
    }
    if (header->mPlaceOffset == 0) {
        result = 18;
        goto fail;
    }

    mForecastEntries = (ForecastEntry*)((u8*)mForecastBin + header->mForecastOffset);
    mSummaryEntries = (SummaryEntry*)((u8*)mForecastBin + mHeader->mSummaryOffset);
    mWeatherTypes = (WeatherType*)((u8*)mForecastBin + mHeader->mWeatherTypeOffset);
    mUVTexts = (IndexText*)((u8*)mForecastBin + mHeader->mUVIndexOffset);
    mLaundryTexts = (IndexText*)((u8*)mForecastBin + mHeader->mLaundryIndexOffset);
    mPollenTexts = (IndexText*)((u8*)mForecastBin + mHeader->mPollenIndexOffset);
    mPlaceEntries = (PlaceEntry*)((u8*)mForecastBin + mHeader->mPlaceOffset);

    if (mHeader->mNumForecasts != 0) {
        mForecasts = new CityForecast[mHeader->mNumForecasts];
        if (mForecasts == NULL) {
            result = 2;
            goto fail;
        }
        CityForecast* forecast = mForecasts;
        ForecastEntry* entry = mForecastEntries;
        for (i = 0; i < mHeader->mNumForecasts; i++, forecast++, entry++) {
            forecast->Setup(mForecastBin, entry);
        }
    }

    if (mHeader->mNumSummaries != 0) {
        mSummaries = new CitySummary[mHeader->mNumSummaries];
        if (mSummaries == NULL) {
            result = 3;
            goto fail;
        }
        CitySummary* summary = mSummaries;
        SummaryEntry* entry = mSummaryEntries;
        for (i = 0; i < mHeader->mNumSummaries; i++, summary++, entry++) {
            summary->Setup(mForecastBin, entry);
        }
    }

    mWeatherInfo = new WeatherInfo[mHeader->mNumWeatherTypes];
    if (mWeatherInfo == NULL) {
        result = 4;
        goto fail;
    }
    {
        WeatherInfo* info = mWeatherInfo;
        WeatherType* type = mWeatherTypes;
        for (i = 0; i < mHeader->mNumWeatherTypes; i++, info++, type++) {
            if (info->Setup(mForecastBin, type) != 0x18) {
                result = 19;
                goto fail;
            }
        }
    }

    if (mHeader->mNumUVIndices != 0) {
        mUVIndices = new UVIndexInfo[mHeader->mNumUVIndices];
        if (mUVIndices == NULL) {
            result = 7;
            goto fail;
        }
        UVIndexInfo* index = mUVIndices;
        IndexText* text = mUVTexts;
        for (i = 0; i < mHeader->mNumUVIndices; i++, index++, text++) {
            index->Setup(mForecastBin, text);
        }
    }

    if (mHeader->mNumLaundryIndices != 0) {
        mLaundryIndices = new LaundryIndexInfo[mHeader->mNumLaundryIndices];
        if (mLaundryIndices == NULL) {
            result = 8;
            goto fail;
        }
        LaundryIndexInfo* index = mLaundryIndices;
        IndexText* text = mLaundryTexts;
        for (i = 0; i < mHeader->mNumLaundryIndices; i++, index++, text++) {
            index->Setup(mForecastBin, text);
        }
    }

    if (mHeader->mNumPollenIndices != 0) {
        mPollenIndices = new PollenIndexInfo[mHeader->mNumPollenIndices];
        if (mPollenIndices == NULL) {
            result = 9;
            goto fail;
        }
        PollenIndexInfo* index = mPollenIndices;
        IndexText* text = mPollenTexts;
        for (i = 0; i < mHeader->mNumPollenIndices; i++, index++, text++) {
            index->Setup(mForecastBin, text);
        }
    }

    mPlaces = new CityInfo[mHeader->mNumPlaces];
    if (mPlaces != NULL) {
        CityInfo* place = mPlaces;
        PlaceEntry* entry = mPlaceEntries;
        for (i = 0; i < mHeader->mNumPlaces; i++, place++, entry++) {
            place->Setup(mForecastBin, entry, i);
        }
        return 0x18;
    }
    result = 5;

fail:
    DELETE_ARRAY(mPlaces);
    DELETE_ARRAY(mPollenIndices);
    DELETE_ARRAY(mLaundryIndices);
    DELETE_ARRAY(mUVIndices);
    DELETE_ARRAY(mWeatherInfo);
    DELETE_ARRAY(mSummaries);
    DELETE_ARRAY(mForecasts);
    if (mForecastBin != NULL) {
        MEM2Free(mForecastBin);
        mForecastBin = NULL;
    }
    return result;
}

s32 ForecastData::LoadShort(void* data) {
    s32 result = 1;

    mShortBin = data;
    if (data == NULL) {
        return result;
    }

    ShortHeader* header = (ShortHeader*)data;
    mShortHeader = header;
    if (header->mNumEntries == 0) {
        result = 14;
        goto fail;
    }
    if (header->mEntryOffset == 0) {
        result = 20;
        goto fail;
    }

    mShortEntries = (ShortEntry*)((u8*)data + header->mEntryOffset);
    if (header->mNumEntries != 0) {
        mNow = new CityNow[header->mNumEntries];
        if (mNow == NULL) {
            result = 6;
            goto fail;
        }
        CityNow* now = mNow;
        ShortEntry* entry = mShortEntries;
        for (u32 i = 0; i < mShortHeader->mNumEntries; i++, now++, entry++) {
            now->Setup(mShortBin, entry);
        }
    }
    return 0x18;

fail:
    MEM2Free(mShortBin);
    mShortBin = NULL;
    return result;
}

CityForecast* ForecastData::FindForecast(const u32& id) {
    CityForecast* forecast = mForecasts;
    for (u32 i = 0; i < mHeader->mNumForecasts; i++, forecast++) {
        if (id == forecast->mEntry->mId) {
            return forecast;
        }
    }
    return NULL;
}

CitySummary* ForecastData::FindSummary(const u32& id) {
    CitySummary* summary = mSummaries;
    for (u32 i = 0; i < mHeader->mNumSummaries; i++, summary++) {
        if (id == summary->mEntry->mId) {
            return summary;
        }
    }
    return NULL;
}

CityNow* ForecastData::FindNow(const u32& id) {
    CityNow* now = mNow;
    for (u32 i = 0; i < mShortHeader->mNumEntries; i++, now++) {
        if (id == now->mEntry->mId) {
            return now;
        }
    }
    return NULL;
}

WeatherInfo* ForecastData::FindWeatherInfo(const u32& code) {
    WeatherInfo* info = mWeatherInfo;
    for (u32 i = 0; i < mHeader->mNumWeatherTypes; i++, info++) {
        if (code == info->mType->mCode) {
            return info;
        }
    }
    return NULL;
}

u16 ForecastData::GetWeatherIcon(const u32& code) {
    WeatherInfo* info = mWeatherInfo;
    u32 i;
    for (i = 0; i < mHeader->mNumWeatherTypes; i++, info++) {
        if (code == info->mType->mCode) {
            goto found;
        }
    }
    info = NULL;
found:
    if (info != NULL) {
        return info->mType->mIcon;
    }
    return 0xFFFF;
}

IndexInfo* ForecastData::FindUVIndex(const u8& code) {
    UVIndexInfo* index = mUVIndices;
    for (u32 i = 0; i < mHeader->mNumUVIndices; i++, index++) {
        if (code == index->mIndex->mCode) {
            return index;
        }
    }
    return NULL;
}

IndexInfo* ForecastData::FindLaundryIndex(const u8& code) {
    LaundryIndexInfo* index = mLaundryIndices;
    for (u32 i = 0; i < mHeader->mNumLaundryIndices; i++, index++) {
        if (code == index->mIndex->mCode) {
            return index;
        }
    }
    return NULL;
}

IndexInfo* ForecastData::FindPollenIndex(const u8& code) {
    PollenIndexInfo* index = mPollenIndices;
    for (u32 i = 0; i < mHeader->mNumPollenIndices; i++, index++) {
        if (code == index->mIndex->mCode) {
            return index;
        }
    }
    return NULL;
}

CityForecast::CityForecast() {
    mEntry = NULL;
}

CityForecast::~CityForecast() {}

void CityForecast::Setup(void* base, ForecastEntry* entry) {
    mEntry = entry;
    mMinutes = (entry->mTime - entry->mOffset) + gStartMinutes;
    MinutesToCalendarTime(mMinutes, &mTime);
}

#define WEATHER_TPL ((TPLPalette*)gUnk80330B74)

// One texture of a weather icon
struct IconLayer {
    u32 mTexture;   // at 0x0, 0xFFFFFFFF ends the list
    Vec2 mOffset;   // at 0x4
    f32 mScale;     // at 0xC
    GXColor mColor; // at 0x10
    GXColor mShadowColor; // at 0x14
};

extern const IconLayer sIcon1[];
extern const IconLayer sIcon2[];
extern const IconLayer sIcon3[];
extern const IconLayer sIcon4[];
extern const IconLayer sIcon5[];
extern const IconLayer sIcon6[];
extern const IconLayer sIcon7[];
extern const IconLayer sIcon8[];
extern const IconLayer sIcon9[];
extern const IconLayer sIcon10[];
extern const IconLayer sIcon11[];
extern const IconLayer sIcon12[];
extern const IconLayer sIcon13[];
extern const IconLayer sIcon14[];
extern const IconLayer sIcon15[];
extern const IconLayer sIcon16[];
extern const IconLayer sIcon17[];
extern const IconLayer sIcon18[];
extern const IconLayer sIcon19[];
extern const IconLayer sIcon20[];
extern const IconLayer sIcon21[];
extern const IconLayer sIcon22[];
extern const IconLayer sIcon23[];
extern const IconLayer sIcon24[];
extern const IconLayer sIcon25[];
extern const IconLayer sIcon26[];
extern const IconLayer sIcon27[];
extern const IconLayer sIcon28[];
extern const IconLayer sIcon29[];
extern const IconLayer sIcon30[];
extern const IconLayer sIcon31[];
extern const IconLayer sIcon32[];
extern const IconLayer sIcon33[];
extern const IconLayer sIcon101[];
extern const IconLayer sIcon102[];
extern const IconLayer sIcon103[];
extern const IconLayer sIcon104[];
extern const IconLayer sIcon105[];
extern const IconLayer sIcon106[];
extern const IconLayer sIcon107[];
extern const IconLayer sIcon108[];
extern const IconLayer sIcon109[];
extern const IconLayer sIcon110[];
extern const IconLayer sIcon111[];
extern const IconLayer sIcon112[];
extern const IconLayer sIcon113[];
extern const IconLayer sIcon114[];
extern const IconLayer sIcon115[];
extern const IconLayer sIcon116[];
extern const IconLayer sIcon117[];
extern const IconLayer sIcon118[];
extern const IconLayer sIcon119[];
extern const IconLayer sIcon120[];
extern const IconLayer sIcon121[];
extern const IconLayer sIcon122[];
extern const IconLayer sIcon123[];
extern const IconLayer sIcon124[];
extern const IconLayer sIcon125[];
extern const IconLayer sIcon126[];

static void DrawIconLayersShadow(const IconLayer* layers, const Vec2* pos, s32 alpha, BOOL night, f32 scale);
static void DrawIconLayers(const IconLayer* layers, const Vec2* pos, s32 alpha, BOOL night, f32 scale);
static void DrawIconTextureShadow(u32 texture, const Vec2* pos, GXColor color, GXColor shadow, f32 scale);
static void DrawIconTexture(u32 texture, const Vec2* pos, GXColor color, f32 scale);

void DrawWeatherIcon(u16 icon, const Vec2* pos, s32 alpha, f32 scale) {
    const IconLayer* layers;

    SetDefaultGXState();
    SetOrthoProjection();
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);

    u32 id = icon & 0x7FFF;
    BOOL night = icon & 0x8000;
    switch (id) {
    case 1:
        layers = sIcon1;
        break;
    case 2:
        layers = sIcon2;
        break;
    case 3:
        layers = sIcon3;
        break;
    case 4:
        layers = sIcon4;
        break;
    case 5:
        layers = sIcon5;
        break;
    case 6:
        layers = sIcon6;
        break;
    case 7:
        layers = sIcon7;
        break;
    case 8:
        layers = sIcon8;
        break;
    case 9:
        layers = sIcon9;
        break;
    case 10:
        layers = sIcon10;
        break;
    case 11:
        layers = sIcon11;
        break;
    case 12:
        layers = sIcon12;
        break;
    case 13:
        layers = sIcon13;
        break;
    case 14:
        layers = sIcon14;
        break;
    case 15:
        layers = sIcon15;
        break;
    case 16:
        layers = sIcon16;
        break;
    case 17:
        layers = sIcon17;
        break;
    case 18:
        layers = sIcon18;
        break;
    case 19:
        layers = sIcon19;
        break;
    case 20:
        layers = sIcon20;
        break;
    case 21:
        layers = sIcon21;
        break;
    case 22:
        layers = sIcon22;
        break;
    case 23:
        layers = sIcon23;
        break;
    case 24:
        layers = sIcon24;
        break;
    case 25:
        layers = sIcon25;
        break;
    case 26:
        layers = sIcon26;
        break;
    case 27:
        layers = sIcon27;
        break;
    case 28:
        layers = sIcon28;
        break;
    case 29:
        layers = sIcon29;
        break;
    case 30:
        layers = sIcon30;
        break;
    case 31:
        layers = sIcon31;
        break;
    case 32:
        layers = sIcon32;
        break;
    case 33:
        layers = sIcon33;
        break;
    case 101:
        layers = sIcon101;
        break;
    case 102:
        layers = sIcon102;
        break;
    case 103:
        layers = sIcon103;
        break;
    case 104:
        layers = sIcon104;
        break;
    case 105:
        layers = sIcon105;
        break;
    case 106:
        layers = sIcon106;
        break;
    case 107:
        layers = sIcon107;
        break;
    case 108:
        layers = sIcon108;
        break;
    case 109:
        layers = sIcon109;
        break;
    case 110:
        layers = sIcon110;
        break;
    case 111:
        layers = sIcon111;
        break;
    case 112:
        layers = sIcon112;
        break;
    case 113:
        layers = sIcon113;
        break;
    case 114:
        layers = sIcon114;
        break;
    case 115:
        layers = sIcon115;
        break;
    case 116:
        layers = sIcon116;
        break;
    case 117:
        layers = sIcon117;
        break;
    case 118:
        layers = sIcon118;
        break;
    case 119:
        layers = sIcon119;
        break;
    case 120:
        layers = sIcon120;
        break;
    case 121:
        layers = sIcon121;
        break;
    case 122:
        layers = sIcon122;
        break;
    case 123:
        layers = sIcon123;
        break;
    case 124:
        layers = sIcon124;
        break;
    case 125:
        layers = sIcon125;
        break;
    case 126:
        layers = sIcon126;
        break;
    default: {
        nw4r::ut::Color color(*(GXColor*)&gColorWhite);
        nw4r::ut::Color shadow(*(GXColor*)&gColorDarkGray);
        shadow.a = alpha;
        color.a = alpha;
        wcscpy(sTextBuf, L"--");
        DrawTempCentered(sTextBuf, pos, scale, scale, &color, &shadow);
        return;
    }
    }

    if (id < 100) {
        DrawIconLayersShadow(layers, pos, alpha, night, scale);
    } else {
        DrawIconLayers(layers, pos, alpha, night, scale);
    }
}

static inline u32 GetNightTexture(u32 texture, BOOL night) {
    switch (texture) {
    case 4:
        return night ? 1 : texture;
    case 15:
        return night ? 11 : texture;
    default:
        return texture;
    }
}

static void DrawIconLayers(const IconLayer* layers, const Vec2* pos, s32 alpha, BOOL night, f32 scale) {
    f32 fade = alpha / 255.0f;
    GXColor color;
    color.r = 255;
    color.g = 255;
    color.b = 255;
    color.a = alpha;
    for (; layers->mTexture != 0xFFFFFFFF; layers++) {
        color.a = layers->mColor.a * fade;
        color.g = layers->mColor.g;
        color.r = layers->mColor.r;
        color.b = layers->mColor.b;
        nw4r::math::VEC2 layerPos(pos->x + layers->mOffset.x * scale, pos->y + layers->mOffset.y * scale);
        DrawIconTexture(GetNightTexture(layers->mTexture, night), (Vec2*)&layerPos, color, layers->mScale * scale);
    }
}

static void DrawIconLayersShadow(const IconLayer* layers, const Vec2* pos, s32 alpha, BOOL night, f32 scale) {
    GXColor color;
    GXColor shadow;
    f32 fade = alpha / 255.0f;
    color.r = 255;
    color.g = 255;
    color.b = 255;
    color.a = alpha;
    shadow.r = 255;
    shadow.g = 255;
    shadow.b = 255;
    shadow.a = alpha;
    for (; layers->mTexture != 0xFFFFFFFF; layers++) {
        color.a = layers->mColor.a * fade;
        color.r = layers->mColor.r;
        color.g = layers->mColor.g;
        color.b = layers->mColor.b;
        shadow.a = layers->mShadowColor.a * fade;
        shadow.r = layers->mShadowColor.r;
        shadow.g = layers->mShadowColor.g;
        shadow.b = layers->mShadowColor.b;
        nw4r::math::VEC2 layerPos(pos->x + layers->mOffset.x * scale, pos->y + layers->mOffset.y * scale);
        DrawIconTextureShadow(GetNightTexture(layers->mTexture, night), (Vec2*)&layerPos, color, shadow,
                              layers->mScale * scale);
    }
}

static void DrawIconTextureShadow(u32 texture, const Vec2* pos, GXColor color, GXColor shadow, f32 scale) {
    f32 halfW = scale * (0.5f * GetTexWidth(WEATHER_TPL, texture));
    f32 halfH = scale * (0.5f * GetTexHeight(WEATHER_TPL, texture));
    Vec corner;
    corner.x = pos->x - halfW;
    corner.y = pos->y - halfH;
    corner.z = 0.0f;
    Vec shadowPos;
    shadowPos.x = 2.0f + corner.x;
    shadowPos.y = 2.0f + corner.y;
    shadowPos.z = 0.0f;
    GXSetTevColor(GX_TEVREG0, shadow);
    DrawTextureAt(WEATHER_TPL, texture, scale, scale, &shadowPos);
    GXSetTevColor(GX_TEVREG0, color);
    DrawTextureAt(WEATHER_TPL, texture, scale, scale, &corner);
}

static void DrawIconTexture(u32 texture, const Vec2* pos, GXColor color, f32 scale) {
    f32 halfW = scale * (0.5f * GetTexWidth(WEATHER_TPL, texture));
    f32 halfH = scale * (0.5f * GetTexHeight(WEATHER_TPL, texture));
    Vec corner;
    corner.x = pos->x - halfW;
    corner.y = pos->y - halfH;
    corner.z = 0.0f;
    GXSetTevColor(GX_TEVREG0, color);
    DrawTextureAt(WEATHER_TPL, texture, scale, scale, &corner);
}

s32 LaundryIndexInfo::Setup(void* base, IndexText* entry) {
    mIndex = entry;
    if (entry->mTextOffset == 0) {
        return 0x16;
    }
    mText = (const wchar_t*)((u8*)base + entry->mTextOffset);
    return 0x18;
}

const IconLayer sIcon1[] = {
    {4, {0.0f, 0.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon2[] = {
    {4, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon3[] = {
    {4, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {2, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon4[] = {
    {4, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {3, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon5[] = {
    {4, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {5, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon6[] = {
    {4, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon7[] = {
    {4, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {2, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon8[] = {
    {4, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {3, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon9[] = {
    {4, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {5, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon10[] = {
    {0, {0.0f, 0.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon11[] = {
    {0, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {4, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon12[] = {
    {0, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {2, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon13[] = {
    {0, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {3, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon14[] = {
    {0, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {5, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon15[] = {
    {0, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {4, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon16[] = {
    {0, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {2, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon17[] = {
    {0, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {3, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon18[] = {
    {0, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {5, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon19[] = {
    {2, {0.0f, 0.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon20[] = {
    {2, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {4, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon21[] = {
    {2, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon22[] = {
    {2, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {3, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon23[] = {
    {2, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {4, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon24[] = {
    {2, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon25[] = {
    {2, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {3, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon26[] = {
    {3, {0.0f, 0.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon27[] = {
    {3, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {4, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon28[] = {
    {3, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon29[] = {
    {3, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {4, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon30[] = {
    {3, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon31[] = {
    {3, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {2, {28.0f, 12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon32[] = {
    {3, {-28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {2, {28.0f, -12.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {18, {0.0f, 23.0f}, 1.0f, {255, 192, 0, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon33[] = {
    {5, {0.0f, 0.0f}, 1.0f, {255, 255, 255, 255}, {32, 32, 32, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon101[] = {
    {15, {0.0f, 0.0f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon102[] = {
    {15, {-24.0f, 8.0f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {6, {4.0f, 28.0f}, 1.2f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon103[] = {
    {15, {-15.0f, -23.0f}, 0.8f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {12, {-4.0f, 50.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon104[] = {
    {15, {-15.0f, -23.0f}, 0.8f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {14, {13.5f, 48.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon105[] = {
    {15, {-15.0f, -23.0f}, 0.8f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {12, {-41.0f, 50.5f}, 0.9f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {14, {52.0f, 48.5f}, 0.9f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon106[] = {
    {6, {0.0f, 7.0f}, 1.2f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon107[] = {
    {15, {0.0f, 9.0f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {6, {29.0f, 27.0f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {8, {-55.5f, -16.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon108[] = {
    {12, {-4.0f, 50.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {6, {-49.0f, -25.0f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon109[] = {
    {6, {-49.0f, -25.0f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {14, {13.5f, 48.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon110[] = {
    {12, {-41.0f, 50.5f}, 0.9f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {6, {-49.0f, -25.0f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {14, {52.0f, 48.5f}, 0.9f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon111[] = {
    {12, {-4.0f, 50.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon112[] = {
    {15, {-25.0f, 0.0f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {12, {-4.0f, 50.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon113[] = {
    {12, {-4.0f, 37.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -20.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {6, {47.0f, 14.0f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon114[] = {
    {12, {-41.0f, 50.5f}, 0.9f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {14, {52.0f, 48.5f}, 0.9f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon115[] = {
    {12, {-4.0f, 50.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {13, {13.5f, 48.5f}, 1.0f, {210, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon116[] = {
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {14, {13.5f, 48.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon117[] = {
    {15, {-25.0f, 0.0f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {14, {13.5f, 48.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon118[] = {
    {7, {0.0f, -20.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {14, {-39.0f, 35.0f}, 0.9f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {6, {47.0f, 14.0f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon119[] = {
    {12, {33.5f, 50.5f}, 0.9f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {14, {-39.0f, 48.5f}, 0.9f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon120[] = {
    {12, {33.5f, 50.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {14, {-39.0f, 48.5f}, 0.9f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {13, {72.0f, 51.5f}, 0.9f, {210, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon121[] = {
    {12, {-4.0f, 50.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {13, {13.5f, 48.5f}, 1.0f, {210, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon122[] = {
    {9, {0.0f, 0.0f}, 1.0f, {230, 190, 70, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon123[] = {
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {17, {26.0f, 36.0f}, 1.0f, {255, 255, 255, 120}, {0, 0, 0, 255}},
    {14, {13.5f, 48.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon124[] = {
    {9, {0.0f, 0.0f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon125[] = {
    {12, {4.0f, 50.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {16, {2.5f, 54.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

const IconLayer sIcon126[] = {
    {12, {4.0f, 50.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {7, {0.0f, -7.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {10, {13.5f, 48.5f}, 1.0f, {255, 255, 255, 255}, {0, 0, 0, 255}},
    {0xFFFFFFFF, {0.0f, 0.0f}, 1.0f, {0, 0, 0, 0}, {0, 0, 0, 0}},
};
