# Japanese log translations

The channel's debug output (`OSReport`) and assertion messages (`OSPanic`) are Japanese text in Shift-JIS.
This page lists every Japanese log message found in `src/`, with an English translation.

The source files hold these messages either as literal text or as `\x..` byte escapes.
Both forms build to the same bytes, because the compiler runs with `-enc SJIS` through sjiswrap (see `configure.py`).

Links are relative to this file.

## Messages

| Japanese | Romaji | English |
| --- | --- | --- |
| `%sが見つかりません!!` | ga mitsukarimasen | "%s not found!!" |
| `%sがありません!` | ga arimasen | "%s doesn't exist!" |
| `%sがないです!!` | ga nai desu | "%s is missing!!" |
| `天気情報生成失敗!!` | tenki jōhō seisei shippai | "Failed to generate weather info!!" |
| `天気住所がエラーです!!` | tenki jūsho ga erā desu | "Weather address is an error!!" (invalid weather location) |
| `メモリがない！！` | memori ga nai | "Out of memory!!" |

In every row except the last three, `%s` is a layout pane or button name: either a literal name such as `text`, `up` or `life_b`, or a name passed in at runtime.

## Where each message appears

### `が見つかりません!!` ("not found!!")

| Location | Call | Full message |
| --- | --- | --- |
| [WeatherBaseDay.cpp:105](../src/WeatherBaseDay.cpp#L105) | `OSPanic` (d_weather_base_day.cpp, 178) | `text が見つかりません!!` |
| [WeatherBaseDay.cpp:112](../src/WeatherBaseDay.cpp#L112) | `OSReport` | `%sが見つかりません!!` (`sBoxNamesJP[i]`) |
| [WeatherBaseDay.cpp:181](../src/WeatherBaseDay.cpp#L181) | `OSPanic` (d_weather_base_day.cpp, 252) | `text が見つかりません!!` |
| [WeatherBaseDay.cpp:188](../src/WeatherBaseDay.cpp#L188) | `OSReport` | `%sが見つかりません!!` (`sBoxNames[i]`) |
| [WeatherOther.cpp:72](../src/WeatherOther.cpp#L72) | `OSPanic` (d_weather_a_other.cpp, 107) | `text が見つかりません!!` |
| [WeatherOther.cpp:79](../src/WeatherOther.cpp#L79) | `OSReport` | `%sが見つかりません!!` (`sBoxNamesJP[i]`) |
| [WeatherOther.cpp:127](../src/WeatherOther.cpp#L127) | `OSPanic` (d_weather_a_other.cpp, 156) | `life_b が見つかりません!!` |
| [WeatherOther.cpp:133](../src/WeatherOther.cpp#L133) | `OSPanic` | `text が見つかりません!!` |
| [WeatherOther.cpp:140](../src/WeatherOther.cpp#L140) | `OSReport` | `%sが見つかりません!!` (`sBoxNames[i]`) |
| [WeatherWeek.cpp:132](../src/WeatherWeek.cpp#L132) | `OSPanic` (d_weather_a_week.cpp, 409) | `text が見つかりません!!` |
| [WeatherWeek.cpp:139](../src/WeatherWeek.cpp#L139) | `OSReport` | `%sが見つかりません!!` (`sDayPaneNamesJP[i]`) |
| [WeatherWeek.cpp:149](../src/WeatherWeek.cpp#L149) | `OSReport` | `%sが見つかりません!!` (`sBoxNamesJP[i]`) |
| [WeatherWeek.cpp:218](../src/WeatherWeek.cpp#L218) | `OSPanic` (d_weather_a_week.cpp, 499) | `text が見つかりません!!` |
| [WeatherWeek.cpp:226](../src/WeatherWeek.cpp#L226) | `OSReport` | `%sが見つかりません!!` (`sBoxNames[i]`) |
| [WeatherNormal.cpp:410](../src/WeatherNormal.cpp#L410) | `OSReport` (in a macro) | `%sが見つかりません!!` (`names[i]`) |
| [WeatherAround.cpp:222](../src/WeatherAround.cpp#L222) | `OSReport` | `WARNING!! ...%sが見つかりません!!` (`*name`), followed by an empty `OSPanic` (d_weather_around.cpp, 298) |

### `がありません!` ("doesn't exist!")

| Location | Call | Full message |
| --- | --- | --- |
| [WeatherNormal.cpp:191](../src/WeatherNormal.cpp#L191) | `OSPanic` (d_weather_normal.cpp, 268) | `aroundがありません!` |
| [WeatherNormal.cpp:196](../src/WeatherNormal.cpp#L196) | `OSPanic` | `setがありません!` |
| [WeatherNormal.cpp:201](../src/WeatherNormal.cpp#L201) | `OSPanic` | `backがありません!` |
| [WeatherNormal.cpp:206](../src/WeatherNormal.cpp#L206) | `OSPanic` | `upがありません!` |
| [WeatherNormal.cpp:211](../src/WeatherNormal.cpp#L211) | `OSPanic` | `downがありません!` |

### `がないです!!` ("is missing!!")

| Location | Call | Full message |
| --- | --- | --- |
| [WeatherAddress.cpp:172](../src/WeatherAddress.cpp#L172) | `OSPanic` (d_weather_address.cpp, 180) | `up がないです!!` |
| [WeatherAddress.cpp:177](../src/WeatherAddress.cpp#L177) | `OSPanic` (d_weather_address.cpp, 185) | `down がないです!!` |
| [WeatherAddress.cpp:182](../src/WeatherAddress.cpp#L182) | `OSPanic` (d_weather_address.cpp, 190) | `back がないです!!` |
| [WeatherAddress.cpp:187](../src/WeatherAddress.cpp#L187) | `OSPanic` (d_weather_address.cpp, 195) | `text がないです!!` |
| [WeatherSetting.cpp:29](../src/WeatherSetting.cpp#L29) | `OSPanic` (d_weather_setting.cpp, 97) | `kion_set がないです!!` |
| [WeatherSetting.cpp:35](../src/WeatherSetting.cpp#L35) | `OSPanic` (d_weather_setting.cpp, 104) | `city_set がないです!!` |
| [WeatherSetting.cpp:41](../src/WeatherSetting.cpp#L41) | `OSPanic` (d_weather_setting.cpp, 111) | `city_btn がないです!!` |
| [WeatherSetting.cpp:47](../src/WeatherSetting.cpp#L47) | `OSPanic` (d_weather_setting.cpp, 118) | `kion_btn がないです!!` |
| [WeatherSetting.cpp:70](../src/WeatherSetting.cpp#L70) | `OSPanic` (d_weather_setting.cpp, 153) | `wind_set がないです!!` |
| [WeatherSetting.cpp:76](../src/WeatherSetting.cpp#L76) | `OSPanic` (d_weather_setting.cpp, 160) | `wind_btn がないです!!` |

`kion` (気温) means temperature, so `kion_set` and `kion_btn` are the temperature-unit setting and button.

### Other messages

| Location | Call | Message | English |
| --- | --- | --- | --- |
| [WeatherScene.cpp:1228](../src/WeatherScene.cpp#L1228) | `OSReport` | `天気情報生成失敗!!` | "Failed to generate weather info!!" |
| [WeatherScene.cpp:1248](../src/WeatherScene.cpp#L1248) | `OSReport` | `天気情報生成失敗!!` | "Failed to generate weather info!!" |
| [WeatherSetting.cpp:245](../src/WeatherSetting.cpp#L245) | `OSPanic` (d_weather_setting.cpp, 439) | `天気住所がエラーです!!` | "Weather address is an error!!" |
| [SceneBase.cpp:994](../src/SceneBase.cpp#L994) | `OSPanic` (d_scene.cpp, 1699) | `メモリがない！！` | "Out of memory!!" |

## SDK message (not channel code)

[dvdFatal.c:11-14](../src/revolution/DVD/dvdFatal.c#L11-L14) holds the Japanese text of the SDK's DVD fatal error screen (`__DVDErrorMessage`, `SC_LANG_JP`):

> エラーが発生しました。
> イジェクトボタンを押してディスクを取り出してから、本体の電源をOFFにして、本体の取扱説明書の指示に従ってください。

"An error has occurred. Press the Eject Button, remove the disc, turn the power off, and follow the instructions in the Wii operations manual."
The same file has its own English version (`SC_LANG_EN`) right after it.
