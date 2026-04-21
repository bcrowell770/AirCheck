# AirCheck Architecture

AirCheck is a published Pebble app that answers one question quickly: should I go outside right now?

The app keeps the watch UI simple and pushes networking, location, settings, and API handling to PebbleKit JS on the phone.

## Platforms
- Aplite
- Basalt
- Chalk

Basalt remains the primary visual reference, with Aplite and Chalk supported through conservative layout and color fallbacks.

## Current Release
v1.2 is published.

Current v1.2 scope includes:
- AQI-based recommendation baseline
- Thunderstorm/hail safety override
- Temperature safety downgrades
- UV awareness
- Manual refresh feedback
- User-selectable temperature units through Clay settings

## Watch App
Primary file: `src/c/main.c`

Responsibilities:
- Render the existing 3-screen UI
- Handle Up/Down navigation
- Handle Select manual refresh
- Receive AppMessage payloads from PebbleKit JS
- Map AQI, weather, temperature, and UV into the final recommendation
- Display compact, readable state across Aplite, Basalt, and Chalk

Screens:
- Summary
- Air Quality
- Weather

The Weather screen shows temperature, condition, and compact UV text such as `UV: 7`.

## Phone Companion
Primary files:
- `src/pkjs/index.js`
- `src/pkjs/config.js`

Responsibilities:
- Request phone geolocation with timeout/error handling
- Fetch Open-Meteo weather and air quality data
- Cache the last successful payload
- Send compact AppMessage payloads to the watch
- Provide Clay settings for temperature units only

## Data Sources
Weather API:
- Open-Meteo Weather API
- Uses phone coordinates
- Requests current `temperature_2m`, `weather_code`, and `uv_index`
- Sends `temperature_unit=fahrenheit` or `temperature_unit=celsius` based on the Clay setting

Air Quality API:
- Open-Meteo Air Quality API
- Uses phone coordinates
- Requests current `us_aqi`

## Recommendation Logic
Priority order:
1. Thunderstorm/hail override forces `STAY IN`.
2. AQI determines the baseline recommendation.
3. Temperature may worsen the recommendation.
4. UV may worsen the recommendation to at least `CAUTION`, but does not force `STAY IN` by itself.

AQI baseline:
- 0 to 50: `GO OUT`
- 51 to 100: `CAUTION`
- 101 and above: `STAY IN`

Dangerous weather:
- WMO weather codes 95 to 99 force `STAY IN`

Temperature thresholds:
- Fahrenheit: 90 to 99 F is at least `CAUTION`; 100 F and above is `STAY IN`
- Fahrenheit: 35 to 44 F is at least `CAUTION`; 34 F and below is `STAY IN`
- Celsius: 27 to 31 C is at least `CAUTION`; 32 C and above is `STAY IN`
- Celsius: 8 to 12 C is at least `CAUTION`; 7 C and below is `STAY IN`

UV threshold:
- UV 0 to 5 has no recommendation impact
- UV 6 and above downgrades to at least `CAUTION`

## Settings
Current v1.2 settings:
- Temperature Unit: Fahrenheit or Celsius

Settings are implemented with Clay and persisted by the Pebble/Rebble settings flow. The selected unit controls the Open-Meteo weather request and the watch-side display/threshold logic.

Not active in v1.2:
- ZIP or fixed-location settings
- Multiple saved locations
- Pollen/allergens
- Background refresh
- AQI trend arrows
- Last-updated timestamps
- Additional screens

## Release History
v1.0 published:
- Initial AQI/weather recommendation app
- Manual refresh
- Cached fallback behavior
- 3-screen watch UI

v1.1 published:
- UV awareness
- Weather-screen UV line
- Simple manual refresh feedback

v1.2 published:
- Temperature-unit setting
- Direct Open-Meteo Fahrenheit/Celsius requests
- Canonical temperature payload handling
- Unit-aware temperature safety thresholds

## Current Posture
AirCheck is in published v1.2 maintenance. Future work should start from a narrowly scoped task and avoid reopening deferred features unless explicitly approved.
