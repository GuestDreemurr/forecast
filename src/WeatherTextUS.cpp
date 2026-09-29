// Wind direction names (US English) and wind speed units
#include <channel/WeatherScene.h>

#pragma dont_reuse_strings on

const wchar_t* gWindDirNamesUS[] = {
    L"\x7121\x98A8", L"\x5317\x5317\x6771", L"\x5317\x6771", L"\x6771\x5317\x6771", L"\x6771", L"\x6771\x5357\x6771",
    L"\x5357\x6771", L"\x5357\x5357\x6771", L"\x5357", L"\x5357\x5357\x897F", L"\x5357\x897F", L"\x897F\x5357\x897F",
    L"\x897F", L"\x897F\x5317\x897F", L"\x5317\x897F", L"\x5317\x5317\x897F", L"\x5317",
    L"Calm", L"NNE", L"NE", L"ENE", L"E", L"ESE", L"SE", L"SSE", L"S", L"SSW", L"SW", L"WSW", L"W", L"WNW", L"NW", L"NNW", L"N",
    L"Windstill", L"NNO", L"NO", L"ONO", L"O", L"OSO", L"SO", L"SSO", L"S", L"SSW", L"SW", L"WSW", L"W", L"WNW", L"NW", L"NNW", L"N",
    L"Pas de vent", L"NNE", L"NE", L"ENE", L"E", L"ESE", L"SE", L"SSE", L"S", L"SSO", L"SO", L"OSO", L"O", L"ONO", L"NO", L"NNO", L"N",
    L"Viento en calma", L"NNE", L"NE", L"ENE", L"E", L"ESE", L"SE", L"SSE", L"S", L"SSO", L"SO", L"OSO", L"O", L"ONO", L"NO", L"NNO", L"N",
    L"Assenza di vento", L"NNE", L"NE", L"ENE", L"E", L"ESE", L"SE", L"SSE", L"S", L"SSO", L"SO", L"OSO", L"O", L"ONO", L"NO", L"NNO", L"N",
    L"Windstil", L"NNO", L"NO", L"ONO", L"O", L"OZO", L"ZO", L"ZZO", L"Z", L"ZZW", L"ZW", L"WZW", L"W", L"WNW", L"NW", L"NNW", L"N",
    NULL,
};

const wchar_t* gWindUnitNames[] = {
    L"mph", L"km/h",
    L"mph", L"km/h",
    L"mi/h", L"km/h",
    L"mph", L"km/h",
    L"millas/h", L"km/h",
    L"mph", L"km/h",
    L"mijl/u", L"km/u",
};

const wchar_t* gWindUnitNames2[] = {
    L"mph", L"km/h",
    L"mph", L"km/h",
    L"mi/h", L"km/h",
    L"mph", L"km/h",
    L"millas/h", L"km/h",
    L"mph", L"km/h",
    L"mijl/u", L"km/u",
};

#pragma dont_reuse_strings reset
