var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var payloadCache = null;

var StatusCode = {
  OK: 0,
  LOADING: 1,
  LOCATION_UNAVAILABLE: 2,
  UPDATE_FAILED: 3
};

var Flags = {
  HAS_AQI: 1 << 0,
  HAS_WEATHER: 1 << 1,
  USING_CACHE: 1 << 2
};

var DEFAULT_SETTINGS = {
  temperatureUnit: 'f',
  aqiScale: 'us',
  locationMode: 'phone',
  fixedLocation: '',
  countryCode: ''
};

var currentSettings = copySettings(DEFAULT_SETTINGS);

function copySettings(settings) {
  return {
    temperatureUnit: settings.temperatureUnit,
    aqiScale: settings.aqiScale,
    locationMode: settings.locationMode,
    fixedLocation: settings.fixedLocation,
    countryCode: settings.countryCode
  };
}

function cleanText(value) {
  return typeof value === 'string' ? value.replace(/^\s+|\s+$/g, '') : '';
}

function normalizeSettings(settings) {
  var normalized = copySettings(DEFAULT_SETTINGS);

  if (settings && (settings.temperatureUnit === 'f' || settings.temperatureUnit === 'c')) {
    normalized.temperatureUnit = settings.temperatureUnit;
  }
  if (settings && (settings.aqiScale === 'us' || settings.aqiScale === 'eu')) {
    normalized.aqiScale = settings.aqiScale;
  }
  if (settings && settings.locationMode === 'fixed') {
    normalized.locationMode = 'fixed';
  }
  if (settings) {
    normalized.fixedLocation = cleanText(settings.fixedLocation);
    normalized.countryCode = cleanText(settings.countryCode).toUpperCase().slice(0, 2);
  }

  return normalized;
}

function settingsAffectData(left, right) {
  return left.temperatureUnit !== right.temperatureUnit ||
    left.aqiScale !== right.aqiScale ||
    left.locationMode !== right.locationMode ||
    left.fixedLocation !== right.fixedLocation ||
    left.countryCode !== right.countryCode;
}

function readSettings() {
  try {
    var raw = localStorage.getItem('aircheck.settings');
    if (raw) return normalizeSettings(JSON.parse(raw));

    raw = localStorage.getItem('aircheck.temperatureUnit');
    if (raw === 'f' || raw === 'c') {
      var migrated = copySettings(DEFAULT_SETTINGS);
      migrated.temperatureUnit = raw;
      return migrated;
    }
  } catch (error) {
    console.log('Settings read failed: ' + error);
  }

  return copySettings(DEFAULT_SETTINGS);
}

function writeSettings(settings) {
  var previous = currentSettings;
  currentSettings = normalizeSettings(settings);

  try {
    localStorage.setItem('aircheck.settings', JSON.stringify(currentSettings));
    localStorage.setItem('aircheck.temperatureUnit', currentSettings.temperatureUnit);
  } catch (error) {
    console.log('Settings write failed: ' + error);
  }

  if (settingsAffectData(previous, currentSettings)) {
    clearCache();
  }
}

function clearCache() {
  payloadCache = null;
  try {
    localStorage.removeItem('aircheck.lastPayload');
  } catch (error) {}
}

function readCache() {
  if (payloadCache) return payloadCache;

  try {
    var raw = localStorage.getItem('aircheck.lastPayload');
    if (!raw) return null;
    payloadCache = JSON.parse(raw);
    return payloadCache;
  } catch (error) {
    console.log('Cache read failed: ' + error);
    return null;
  }
}

function writeCache(payload) {
  payloadCache = payload;
  try {
    localStorage.setItem('aircheck.lastPayload', JSON.stringify(payload));
  } catch (error) {
    console.log('Cache write failed: ' + error);
  }
}

function isFiniteNumber(value) {
  return typeof value === 'number' && isFinite(value);
}

function getJson(url, callback) {
  var req = new XMLHttpRequest();

  req.onload = function() {
    if (req.status < 200 || req.status >= 300) {
      callback(new Error('HTTP ' + req.status));
      return;
    }
    try {
      callback(null, JSON.parse(req.responseText));
    } catch (error) {
      callback(error);
    }
  };
  req.onerror = function() { callback(new Error('Network error')); };
  req.ontimeout = function() { callback(new Error('Timeout')); };
  req.timeout = 15000;
  req.open('GET', url);
  req.send();
}

function buildWeatherUrl(lat, lon) {
  var unit = currentSettings.temperatureUnit === 'c' ? 'celsius' : 'fahrenheit';
  return 'https://api.open-meteo.com/v1/forecast'
    + '?latitude=' + encodeURIComponent(lat)
    + '&longitude=' + encodeURIComponent(lon)
    + '&current=temperature_2m,weather_code,uv_index'
    + '&temperature_unit=' + unit;
}

function buildAirUrl(lat, lon) {
  var field = currentSettings.aqiScale === 'eu' ? 'european_aqi' : 'us_aqi';
  return 'https://air-quality-api.open-meteo.com/v1/air-quality'
    + '?latitude=' + encodeURIComponent(lat)
    + '&longitude=' + encodeURIComponent(lon)
    + '&current=' + field;
}

function buildGeocodingUrl() {
  var url = 'https://geocoding-api.open-meteo.com/v1/search'
    + '?name=' + encodeURIComponent(currentSettings.fixedLocation)
    + '&count=1&language=en&format=json';
  if (currentSettings.countryCode) {
    url += '&countryCode=' + encodeURIComponent(currentSettings.countryCode);
  }
  return url;
}

function parseWeatherResponse(json) {
  if (!json || !json.current ||
      !isFiniteNumber(json.current.temperature_2m) ||
      !isFiniteNumber(json.current.weather_code)) return null;

  return {
    temperature: Math.round(json.current.temperature_2m),
    weatherCode: Math.round(json.current.weather_code),
    uvIndex: isFiniteNumber(json.current.uv_index) ? Math.round(json.current.uv_index) : -1
  };
}

function parseAirResponse(json) {
  var field = currentSettings.aqiScale === 'eu' ? 'european_aqi' : 'us_aqi';
  if (!json || !json.current || !isFiniteNumber(json.current[field])) return null;
  return { aqi: Math.round(json.current[field]) };
}

function sendPayload(payload, statusCode, usingCache) {
  var flags = 0;
  var dictionary = {
    StatusCode: statusCode,
    Flags: 0,
    AQI: 0,
    AQIScale: currentSettings.aqiScale === 'eu' ? 1 : 0,
    Temperature: 0,
    TemperatureUnit: currentSettings.temperatureUnit === 'c' ? 1 : 0,
    WeatherCode: 0,
    UvIndex: -1,
    LocationMode: currentSettings.locationMode === 'fixed' ? 1 : 0
  };

  if (payload && typeof payload.aqi === 'number') {
    dictionary.AQI = payload.aqi;
    flags |= Flags.HAS_AQI;
  }
  if (payload && typeof payload.temperature === 'number') {
    dictionary.Temperature = payload.temperature;
  }
  if (payload && typeof payload.weatherCode === 'number') {
    dictionary.WeatherCode = payload.weatherCode;
    flags |= Flags.HAS_WEATHER;
  }
  if (payload && typeof payload.uvIndex === 'number') {
    dictionary.UvIndex = payload.uvIndex;
  }
  if (usingCache) flags |= Flags.USING_CACHE;
  dictionary.Flags = flags;

  Pebble.sendAppMessage(dictionary,
    function() { console.log('Payload sent'); },
    function(error) { console.log('Send failed: ' + JSON.stringify(error)); }
  );
}

function sendCachedOrStatus(statusCode) {
  var cached = readCache();
  sendPayload(cached, statusCode, !!cached);
}

function fetchConditions(lat, lon) {
  var weatherDone = false;
  var airDone = false;
  var weatherResult = null;
  var airResult = null;

  function maybeFinish() {
    var payload;
    if (!weatherDone || !airDone) return;
    if (!airResult) {
      sendCachedOrStatus(StatusCode.UPDATE_FAILED);
      return;
    }

    payload = { aqi: airResult.aqi };
    if (weatherResult) {
      payload.temperature = weatherResult.temperature;
      payload.weatherCode = weatherResult.weatherCode;
      payload.uvIndex = weatherResult.uvIndex;
    }
    writeCache(payload);
    sendPayload(payload, StatusCode.OK, false);
  }

  getJson(buildWeatherUrl(lat, lon), function(error, json) {
    if (!error) weatherResult = parseWeatherResponse(json);
    weatherDone = true;
    maybeFinish();
  });
  getJson(buildAirUrl(lat, lon), function(error, json) {
    if (!error) airResult = parseAirResponse(json);
    airDone = true;
    maybeFinish();
  });
}

function refreshFixedLocation() {
  if (!currentSettings.fixedLocation) {
    sendCachedOrStatus(StatusCode.LOCATION_UNAVAILABLE);
    return;
  }

  getJson(buildGeocodingUrl(), function(error, json) {
    var result;
    if (error || !json || !json.results || !json.results.length) {
      sendCachedOrStatus(StatusCode.LOCATION_UNAVAILABLE);
      return;
    }
    result = json.results[0];
    if (!isFiniteNumber(result.latitude) || !isFiniteNumber(result.longitude)) {
      sendCachedOrStatus(StatusCode.LOCATION_UNAVAILABLE);
      return;
    }
    fetchConditions(result.latitude, result.longitude);
  });
}

function refreshData() {
  sendCachedOrStatus(StatusCode.LOADING);

  if (currentSettings.locationMode === 'fixed') {
    refreshFixedLocation();
    return;
  }

  navigator.geolocation.getCurrentPosition(function(pos) {
    if (!pos || !pos.coords) {
      sendCachedOrStatus(StatusCode.LOCATION_UNAVAILABLE);
      return;
    }
    fetchConditions(pos.coords.latitude, pos.coords.longitude);
  }, function() {
    sendCachedOrStatus(StatusCode.LOCATION_UNAVAILABLE);
  });
}

Pebble.addEventListener('ready', function() {
  currentSettings = readSettings();
  refreshData();
});

Pebble.addEventListener('appmessage', function(event) {
  if (event.payload && event.payload.RequestRefresh) refreshData();
});

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(event) {
  var settings;
  if (!event || !event.response || event.response === 'CANCELLED') return;

  try {
    settings = clay.getSettings(event.response, false);
    writeSettings({
      temperatureUnit: settings.TemperatureUnit,
      aqiScale: settings.AQIScale,
      locationMode: settings.LocationMode,
      fixedLocation: settings.FixedLocation,
      countryCode: settings.CountryCode
    });
    refreshData();
  } catch (error) {
    console.log('Config parse failed: ' + error);
  }
});
