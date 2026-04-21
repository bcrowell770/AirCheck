# AGENTS.md

## Project
AirCheck is a multi-platform Pebble app targeting:
- Aplite
- Basalt
- Chalk

It is an existing published app. v1.2 is the current published release. Prefer minimal, safe, targeted changes over broad refactors.

## Current release status
- v1.2 is published.
- Supported platforms are Aplite, Basalt, and Chalk.
- Current settings support is limited to temperature units: Fahrenheit or Celsius.
- ZIP/fixed-location settings, pollen, timestamps, trend arrows, background refresh, and extra screens are not active scope unless a future task explicitly reopens them.

## Primary files
- `src/c/main.c` - watch app UI, screen rendering, recommendation logic, AppMessage handling
- `src/pkjs/index.js` - PebbleKit JS companion, settings handling, API requests, cache, message payloads
- `src/pkjs/config.js` - Clay settings UI schema

## Build expectations
When making changes:
1. Keep the Pebble build passing for all platforms
2. Avoid introducing syntax that the Pebble JS toolchain cannot parse
3. Preserve existing behavior unless the task explicitly requires changing it

## Working style
- Make minimal diffs
- Do not redesign UI unless explicitly asked
- Do not remove existing functionality unless explicitly asked
- Keep naming and structure consistent with the current codebase
- Prefer fixing root causes over layering additional complexity
- If legacy code is unrelated to the requested fix, leave it alone unless it directly blocks the task

## Pebble JS constraints
- Use plain JavaScript compatible with the Pebble/Rebble toolchain
- Do not paste markdown or prose into `.js` files
- Avoid unnecessary modern syntax if the existing file uses older style JavaScript

## App behavior constraints
Unless the task explicitly says otherwise:
- Do not change the 3-screen structure
- Do not modify AQI recommendation behavior
- Do not modify UV logic
- Do not modify thunderstorm override behavior
- Do not change refresh behavior except where needed to fix the requested issue
- Do not add new features

## Settings
- Clay settings should be treated as the authoritative source for user-configured values
- Keep settings handling simple and consistent
- Avoid duplicate sources of truth for the same setting unless there is a clear reason
- Do not reintroduce ZIP/location settings unless explicitly requested

## Payload conventions
When working with JS-to-watch payloads:
- Keep payload fields predictable and stable
- Prefer one canonical field per value
- Avoid mixed formats for the same concept
- Numeric values should remain numeric unless the watch explicitly expects strings

## Cache conventions
- Cache should mirror live payload structure as closely as possible
- Do not store multiple parallel representations of the same value
- If a setting change invalidates cached data, clear or refresh cache appropriately

## Validation
For any change:
- Build the app
- Confirm the specific requested behavior is fixed
- Check for regressions in adjacent behavior
- Report exactly what changed and why

## Scope guardrails
For AirCheck, common sources of accidental scope creep include:
- old ZIP/location code
- UI polish requests not part of the current task
- watch-side renames that are cosmetic only
- large cleanup passes unrelated to the bug being fixed

Do not expand into those unless explicitly requested.
