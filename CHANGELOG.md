# Changelog

## [Unreleased]

## [v1.3.1] - 2026-07-18
### Added
- Added United States and European AQI scale selection in Clay settings.
- Added optional fixed-location lookup by city or postal code and country code.
- Added native Diorite, Emery, Flint, and Gabbro builds.

### Changed
- Added scale-aware AQI categories, guidance, and recommendation thresholds.
- Added native high-resolution layouts for Emery and Gabbro.
- Use the Chalk verdict font on Chalk, Emery, and Gabbro.
- Updated the project build script for the current seven-platform Pebble SDK.

### Fixed
- Corrected the app UUID to match the existing published AirCheck listing.

## [v1.2] - 2026-04-20
### Added
- Added Clay settings for temperature units: Fahrenheit or Celsius.
- Added unit-aware temperature display on the Summary and Weather screens.
- Added unit-aware temperature safety thresholds.

### Changed
- Weather requests now ask Open-Meteo for the selected temperature unit directly.
- The phone payload now uses a single canonical temperature value plus a temperature-unit flag.
- Cached weather data is refreshed when the temperature unit changes.

### Fixed
- Fixed Celsius/Fahrenheit switching so saved Clay settings are used as the authoritative source.
- Fixed weather payload gating after the temperature payload was simplified.

## [v1.1]
### Added
- Added UV index to the Open-Meteo weather data flow.
- Added UV-based recommendation downgrades for high UV.
- Added compact UV display on the Weather screen.
- Added simple manual refresh feedback.

### Changed
- Kept settings disabled for v1.1.
- Kept timestamp and AQI trend experiments out of the release.

## [v1.0]
### Added
- Added initial 3-screen AirCheck watch UI.
- Added phone-side geolocation, weather fetch, AQI fetch, and cached fallback behavior.
- Added AQI recommendation logic with thunderstorm/hail override support.
- Added manual refresh through the Select button.
