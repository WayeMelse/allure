# Allure

A workout app for the **Pebble Time 2** (platform `emery`): walk, run or train
with your heart rate shown live in five colour-coded effort zones.

Français : [README.fr.md](README.fr.md)

## Features

- Three activities: walk, run, workout, with a 3-second countdown.
- Live heart rate. The top half of the screen changes colour with your effort
  zone (five zones, as a percentage of your estimated maximum heart rate).
- Duration, steps, distance, pace and speed. Press the down button to switch.
- Session summary: average and maximum heart rate, estimated calories.
- Settings on the phone: sex, age, height and weight.
- The language follows the watch language: English, French, German, Spanish,
  Italian and Portuguese.

## How the numbers are computed

- Maximum heart rate: Tanaka formula (208 - 0.7 x age) for men, Gulati formula
  (206 - 0.88 x age) for women.
- Zones: below 60 %, 60-70 %, 70-80 %, 80-90 % and 90 % or more of that maximum.
- Calories: Keytel formula, from average heart rate, weight, age and sex.
- Distance: steps multiplied by a stride length estimated from your height.
  There is no GPS, so distance, pace and speed are estimates.

## Requirements

- A Pebble Time 2 (heart rate sensor required).
- The Pebble SDK, with the `emery` platform.

## Build and install

```bash
pebble package install @rebble/clay   # settings page (first time only)
pebble build
pebble install --emulator emery       # emulator
pebble install --phone <phone-ip>     # real watch, developer connection on
```

## Settings

Open the app in the Pebble mobile app and tap the gear icon. Values are stored
on your phone and on your watch. Until you save them once, defaults are used
(male, 35 years, 175 cm, 75 kg).

## Status and limitations

- Tested on a Pebble Time 2 and in the emulator.
- Metric units only (kg, cm, km).
- The German, Spanish, Italian and Portuguese texts were written with
  assistance and have not been reviewed by native speakers: corrections are
  welcome.
- Chinese is not supported yet; a Chinese watch falls back to English.

## Licenses

The source code is released under the [MIT License](LICENSE).
The fonts in `resources/fonts/` keep their own licenses (SIL Open Font
License 1.1): see [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

Author: WayeM
