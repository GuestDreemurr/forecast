#ifndef CHANNEL_WII_CONNECT24_H
#define CHANNEL_WII_CONNECT24_H
#include <types.h>

// A WiiConnect24 request running on the worker thread (size 0x1B8)
struct WC24Task {
    u8 unk0[0x164];   // at 0x0
    s32 mState;       // at 0x164
    s32 mResult;      // at 0x168
    s32 mErrorCode;   // at 0x16C
    s32 mNwc24Result; // at 0x170
    u8 unk174[0x1B8 - 0x174];
};

extern WC24Task gWC24Tasks[];

// WiiConnect24.cpp: weather download tasks
class CWiiConnect24 {
public:
    static s32 getWeather(void* forecastBuf, s64* forecastTime, u32* forecastSize, void* shortBuf,
                          s64* shortTime, u32* shortSize, const char* forecastUrl, const char* shortUrl,
                          u32 forecastBufSize, u32 forecastMaxSize, u32 shortBufSize);
    static s32 downloadWeatherForecast(void* buf, s64* time, u32* size, u32 bufSize);
    static s32 downloadWeatherShort(void* buf, s64* time, u32* size, u32 bufSize);
    static s32 setWeather(const char* forecastUrl, const char* shortUrl, u32 forecastBufSize, u32 arg3,
                          u32 arg4);
    static s32 deleteWeather();
};

#endif
