# AirCheck Project State

## Current Status
AirCheck v1.2 is published.

The app is in maintenance mode for the current release. There is no active implementation task unless a new scoped task is provided.

## Supported Platforms
- Aplite
- Basalt
- Chalk

Basalt remains the primary design reference, with Aplite and Chalk supported through targeted compatibility work.

## Published Feature Set
- 3-screen Pebble watch UI: Summary, Air Quality, Weather
- Manual refresh with simple `Refreshing...` feedback
- Phone-side geolocation through PebbleKit JS
- Open-Meteo weather and air quality requests
- Cached fallback for the last successful payload
- AQI baseline recommendation logic
- Thunderstorm/hail override for WMO codes 95 to 99
- Temperature safety downgrades
- UV awareness
- Clay settings for temperature units only
- Direct Open-Meteo Fahrenheit/Celsius weather requests

## Recommendation Logic
Priority order:
1. Thunderstorm/hail override forces `STAY IN`.
2. AQI determines the baseline recommendation.
3. Temperature may worsen the recommendation.
4. UV 6 and above may worsen the recommendation to at least `CAUTION`.

Temperature thresholds are unit-aware:
- Fahrenheit: 90 to 99 F is at least `CAUTION`; 100 F and above is `STAY IN`
- Fahrenheit: 35 to 44 F is at least `CAUTION`; 34 F and below is `STAY IN`
- Celsius: 27 to 31 C is at least `CAUTION`; 32 C and above is `STAY IN`
- Celsius: 8 to 12 C is at least `CAUTION`; 7 C and below is `STAY IN`

## Release History
v1.0 published:
- Initial AirCheck release with AQI/weather recommendation flow

v1.1 published:
- Added UV awareness and simple refresh feedback

v1.2 published:
- Added user-selectable Fahrenheit/Celsius temperature units
- Updated weather requests to ask Open-Meteo for the selected unit directly
- Kept ZIP/fixed-location settings out of the release

## Deferred Scope
These are not active unless explicitly approved in a future task:
- ZIP or fixed-location settings
- Multiple saved locations
- Pollen/allergens
- Background refresh
- AQI trend arrows
- Last-updated timestamps
- Additional screens
- Broader severe-weather handling

## Next Recommended Task
No active code task. Continue with targeted v1.2 maintenance only, such as confirmed regression fixes or documentation corrections.
