var Clay = require('@rebble/clay');
var clayConfig = require('./config');

var clay = new Clay(clayConfig);

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

var DEFAULT_TEMPERATURE_UNIT = 'f';
var currentTemperatureUnit = DEFAULT_TEMPERATURE_UNIT;

/* =========================
   TEMPERATURE UNIT HANDLING
   ========================= */

function isValidTemperatureUnit(value) {
  return value === 'f' || value === 'c';
}

function readTemperatureUnit() {
  var raw;

  try {
    raw = localStorage.getItem('aircheck.temperatureUnit');
    if (isValidTemperatureUnit(raw)) {
      return raw;
    }
  } catch (error) {
    console.log('Temperature unit read failed: ' + error);
  }

  return DEFAULT_TEMPERATURE_UNIT;
}

function writeTemperatureUnit(unit) {
  var previous = currentTemperatureUnit;

  if (!isValidTemperatureUnit(unit)) {
    unit = DEFAULT_TEMPERATURE_UNIT;
  }

  currentTemperatureUnit = unit;

  try {
    localStorage.setItem('aircheck.temperatureUnit', unit);
  } catch (error) {
    console.log('Temperature unit write failed: ' + error);
  }

  if (previous !== unit) {
    payloadCache = null;
    try {
      localStorage.removeItem('aircheck.lastPayload');
    } catch (e) {}
  }

  console.log('Temperature unit active: ' + currentTemperatureUnit);
}

/* =========================
   CACHE
   ========================= */

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

/* =========================
   HELPERS
   ========================= */

function isFiniteNumber(value) {
  return typeof value === 'number' && isFinite(value);
}

function roundedValue(value) {
  return Math.round(value);
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
    } catch (e) {
      callback(e);
    }
  };

  req.onerror = function() { callback(new Error('Network error')); };
  req.ontimeout = function() { callback(new Error('Timeout')); };

  req.timeout = 15000;
  req.open('GET', url);
  req.send();
}

/* =========================
   API BUILDERS
   ========================= */

function buildWeatherUrl(lat, lon) {
  var unit = currentTemperatureUnit === 'c' ? 'celsius' : 'fahrenheit';

  return 'https://api.open-meteo.com/v1/forecast'
    + '?latitude=' + encodeURIComponent(lat)
    + '&longitude=' + encodeURIComponent(lon)
    + '&current=temperature_2m,weather_code,uv_index'
    + '&temperature_unit=' + unit;
}

function buildAirUrl(lat, lon) {
  return 'https://air-quality-api.open-meteo.com/v1/air-quality'
    + '?latitude=' + encodeURIComponent(lat)
    + '&longitude=' + encodeURIComponent(lon)
    + '&current=us_aqi';
}

/* =========================
   PARSERS
   ========================= */

function parseWeatherResponse(json) {
  if (!json || !json.current) return null;

  if (!isFiniteNumber(json.current.temperature_2m) ||
      !isFiniteNumber(json.current.weather_code)) {
    return null;
  }

  return {
    temperature: roundedValue(json.current.temperature_2m),
    weatherCode: roundedValue(json.current.weather_code),
    uvIndex: isFiniteNumber(json.current.uv_index)
      ? roundedValue(json.current.uv_index)
      : -1
  };
}

function parseAirResponse(json) {
  if (!json || !json.current || !isFiniteNumber(json.current.us_aqi)) {
    return null;
  }

  return {
    aqi: roundedValue(json.current.us_aqi)
  };
}

/* =========================
   PAYLOAD
   ========================= */

function sendPayload(payload, statusCode, usingCache) {
  var flags = 0;

  var dictionary = {
    StatusCode: statusCode,
    Flags: 0,
    AQI: 0,
    Temperature: 0,
    TemperatureUnit: currentTemperatureUnit === 'c' ? 1 : 0,
    WeatherCode: 0,
    UvIndex: -1
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

  if (usingCache) {
    flags |= Flags.USING_CACHE;
  }

  dictionary.Flags = flags;

  Pebble.sendAppMessage(dictionary,
    function() { console.log('Payload sent'); },
    function(e) { console.log('Send failed: ' + JSON.stringify(e)); }
  );
}

function sendCachedOrStatus(statusCode) {
  var cached = readCache();
  if (cached) {
    sendPayload(cached, statusCode, true);
    return;
  }

  sendPayload(null, statusCode, false);
}

/* =========================
   FETCH
   ========================= */

function fetchConditions(lat, lon) {
  var weatherDone = false;
  var airDone = false;
  var weatherResult = null;
  var airResult = null;

  function maybeFinish() {
    if (!weatherDone || !airDone) return;

    if (!airResult) {
      sendCachedOrStatus(StatusCode.UPDATE_FAILED);
      return;
    }

    var payload = { aqi: airResult.aqi };

    if (weatherResult) {
      payload.temperature = weatherResult.temperature;
      payload.weatherCode = weatherResult.weatherCode;
      payload.uvIndex = weatherResult.uvIndex;
    }

    writeCache(payload);
    sendPayload(payload, StatusCode.OK, false);
  }

  getJson(buildWeatherUrl(lat, lon), function(err, json) {
    if (!err) weatherResult = parseWeatherResponse(json);
    weatherDone = true;
    maybeFinish();
  });

  getJson(buildAirUrl(lat, lon), function(err, json) {
    if (!err) airResult = parseAirResponse(json);
    airDone = true;
    maybeFinish();
  });
}

function refreshData() {
  sendCachedOrStatus(StatusCode.LOADING);

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

/* =========================
   EVENTS
   ========================= */

Pebble.addEventListener('ready', function() {
  currentTemperatureUnit = readTemperatureUnit();
  console.log('Ready, unit: ' + currentTemperatureUnit);
  refreshData();
});

Pebble.addEventListener('appmessage', function(e) {
  if (e.payload && e.payload.RequestRefresh) {
    refreshData();
  }
});

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response || e.response === 'CANCELLED') return;

  try {
    var settings = clay.getSettings(e.response);

    console.log('Raw settings:', JSON.stringify(settings));

    // 🔥 THIS IS THE FIX
    var unit = settings.TemperatureUnit || settings['10010'];

    console.log('Parsed unit:', unit);

    writeTemperatureUnit(unit);
    refreshData();

  } catch (err) {
    console.log('Config parse failed: ' + err);
  }
});