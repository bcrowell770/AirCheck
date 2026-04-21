#include <pebble.h>

#define SCREEN_COUNT 3

enum AirCheckStatus {
  AIRCHECK_STATUS_OK = 0,
  AIRCHECK_STATUS_LOADING = 1,
  AIRCHECK_STATUS_LOCATION_UNAVAILABLE = 2,
  AIRCHECK_STATUS_UPDATE_FAILED = 3
};

enum PayloadFlags {
  FLAG_HAS_AQI = 1 << 0,
  FLAG_HAS_WEATHER = 1 << 1,
  FLAG_USING_CACHE = 1 << 2
};

typedef struct {
  bool has_live_data;
  bool has_aqi;
  bool has_weather;
  bool use_celsius;
  bool using_cache;
  bool is_fixed_location;
  int status_code;
  int aqi;
  int temperature;
  int uv_index;
  int weather_code;
  char fixed_zip[8];
} LivePayload;

typedef enum {
  RECOMMENDATION_NO_DATA = 0,
  RECOMMENDATION_GO_OUT = 1,
  RECOMMENDATION_CAUTION = 2,
  RECOMMENDATION_STAY_IN = 3
} RecommendationLevel;

static Window *s_main_window;
static TextLayer *s_title_layer;
static TextLayer *s_primary_layer;
static TextLayer *s_secondary_layer;
static TextLayer *s_detail_layer;
static TextLayer *s_reason_layer;
static TextLayer *s_helper_layer;

static int s_screen_index = 0;
static bool s_is_loading = true;
static bool s_manual_refresh_in_progress = false;
static LivePayload s_live_payload = {
  .has_live_data = false,
  .has_aqi = false,
  .has_weather = false,
  .use_celsius = false,
  .using_cache = false,
  .is_fixed_location = false,
  .status_code = AIRCHECK_STATUS_LOADING,
  .aqi = 0,
  .temperature = 0,
  .uv_index = -1,
  .weather_code = 0,
  .fixed_zip = ""
};

static TextLayer *create_text_layer(GRect frame, GFont font, GTextAlignment alignment) {
  TextLayer *layer = text_layer_create(frame);
  text_layer_set_background_color(layer, GColorBlack);
  text_layer_set_text_color(layer, GColorWhite);
  text_layer_set_font(layer, font);
  text_layer_set_text_alignment(layer, alignment);
  text_layer_set_overflow_mode(layer, GTextOverflowModeWordWrap);
  return layer;
}

static GRect inset_rect(GRect bounds, int inset_x) {
  return GRect(inset_x, 0, bounds.size.w - (inset_x * 2), bounds.size.h);
}

static bool is_dangerous_weather_code(int weather_code) {
  return weather_code >= 95 && weather_code <= 99;
}

static const char *weather_label_for_code(int weather_code) {
  if(weather_code == 0) {
    return "Clear";
  }
  if(weather_code >= 1 && weather_code <= 2) {
    return "Partly Cloudy";
  }
  if(weather_code == 3) {
    return "Cloudy";
  }
  if(weather_code == 45 || weather_code == 48) {
    return "Fog";
  }
  if(weather_code >= 51 && weather_code <= 67) {
    return "Rain";
  }
  if(weather_code >= 71 && weather_code <= 77) {
    return "Snow";
  }
  if(weather_code >= 80 && weather_code <= 86) {
    return "Showers";
  }
  if(is_dangerous_weather_code(weather_code)) {
    return "Storm";
  }
  return "Unknown";
}

static const char *aqi_category_for_value(int aqi) {
  if(aqi <= 50) {
    return "GOOD";
  }
  if(aqi <= 100) {
    return "MODERATE";
  }
  if(aqi <= 150) {
    return "UNHEALTHY FOR SG";
  }
  if(aqi <= 200) {
    return "UNHEALTHY";
  }
  if(aqi <= 300) {
    return "VERY UNHEALTHY";
  }
  return "HAZARDOUS";
}

static const char *aqi_guidance_for_value(int aqi) {
  if(aqi <= 50) {
    return "Safe for most people";
  }
  if(aqi <= 100) {
    return "Sensitive use care";
  }
  if(aqi <= 150) {
    return "Limit outdoor time";
  }
  if(aqi <= 200) {
    return "Avoid prolonged exposure";
  }
  return "Stay inside if possible";
}

static RecommendationLevel aqi_recommendation_for_payload(const LivePayload *payload) {
  if(!payload->has_aqi) {
    return RECOMMENDATION_NO_DATA;
  }
  if(payload->aqi <= 50) {
    return RECOMMENDATION_GO_OUT;
  }
  if(payload->aqi <= 100) {
    return RECOMMENDATION_CAUTION;
  }
  return RECOMMENDATION_STAY_IN;
}

static RecommendationLevel temperature_recommendation_for_payload(const LivePayload *payload) {
  if(!payload->has_weather) {
    return RECOMMENDATION_NO_DATA;
  }
  if(payload->use_celsius) {
    if(payload->temperature >= 32 || payload->temperature <= 7) {
      return RECOMMENDATION_STAY_IN;
    }
    if((payload->temperature >= 27 && payload->temperature <= 31) ||
       (payload->temperature >= 8 && payload->temperature <= 12)) {
      return RECOMMENDATION_CAUTION;
    }
    return RECOMMENDATION_GO_OUT;
  }
  if(payload->temperature >= 100 || payload->temperature <= 34) {
    return RECOMMENDATION_STAY_IN;
  }
  if((payload->temperature >= 90 && payload->temperature <= 99) ||
     (payload->temperature >= 35 && payload->temperature <= 44)) {
    return RECOMMENDATION_CAUTION;
  }
  return RECOMMENDATION_GO_OUT;
}

static RecommendationLevel uv_recommendation_for_payload(const LivePayload *payload) {
  if(!payload->has_weather || payload->uv_index < 0) {
    return RECOMMENDATION_NO_DATA;
  }
  if(payload->uv_index >= 6) {
    return RECOMMENDATION_CAUTION;
  }
  return RECOMMENDATION_GO_OUT;
}

static RecommendationLevel final_recommendation_for_payload(const LivePayload *payload) {
  RecommendationLevel recommendation = aqi_recommendation_for_payload(payload);
  RecommendationLevel temperature_recommendation = temperature_recommendation_for_payload(payload);
  RecommendationLevel uv_recommendation = uv_recommendation_for_payload(payload);

  if(payload->has_weather && is_dangerous_weather_code(payload->weather_code)) {
    return RECOMMENDATION_STAY_IN;
  }

  if(temperature_recommendation > recommendation) {
    recommendation = temperature_recommendation;
  }
  if(uv_recommendation > recommendation) {
    recommendation = uv_recommendation;
  }

  return recommendation;
}

static const char *verdict_text_for_payload(const LivePayload *payload) {
  RecommendationLevel recommendation = final_recommendation_for_payload(payload);

  if(recommendation == RECOMMENDATION_STAY_IN) {
    return "STAY IN";
  }
  if(recommendation == RECOMMENDATION_GO_OUT) {
    return "GO OUT";
  }
  if(recommendation == RECOMMENDATION_CAUTION) {
    return "CAUTION";
  }
  return "NO DATA";
}

static GColor verdict_color_for_payload(const LivePayload *payload) {
  RecommendationLevel recommendation = final_recommendation_for_payload(payload);

#ifdef PBL_BW
  (void)payload;
  return GColorWhite;
#else
  if(recommendation == RECOMMENDATION_GO_OUT) {
    return GColorIslamicGreen;
  }
  if(recommendation == RECOMMENDATION_CAUTION) {
    return GColorChromeYellow;
  }
  if(recommendation == RECOMMENDATION_STAY_IN) {
    return GColorRed;
  }
  return GColorWhite;
#endif
}

static const char *temperature_reason_for_payload(const LivePayload *payload) {
  if(!payload->has_weather) {
    return NULL;
  }
  if(payload->use_celsius) {
    if(payload->temperature >= 32) {
      return "Very Hot";
    }
    if(payload->temperature >= 27) {
      return "Hot";
    }
    if(payload->temperature <= 7) {
      return "Very Cold";
    }
    if(payload->temperature <= 12) {
      return "Cold";
    }
    return NULL;
  }
  if(payload->temperature >= 100) {
    return "Very Hot";
  }
  if(payload->temperature >= 90) {
    return "Hot";
  }
  if(payload->temperature <= 34) {
    return "Very Cold";
  }
  if(payload->temperature <= 44) {
    return "Cold";
  }
  return NULL;
}

static const char *uv_reason_for_payload(const LivePayload *payload) {
  if(!payload->has_weather || payload->uv_index < 6) {
    return NULL;
  }
  return "High UV";
}

static const char *aqi_reason_for_payload(const LivePayload *payload) {
  if(!payload->has_aqi) {
    return NULL;
  }
  if(payload->aqi <= 50) {
    return "AQI Good";
  }
  if(payload->aqi <= 100) {
    return "AQI Moderate";
  }
  return "AQI Unhealthy";
}

static const char *summary_reason_for_payload(const LivePayload *payload) {
  static char reason_buffer[32];
  const char *aqi_reason;
  const char *temperature_reason;
  const char *uv_reason;
  RecommendationLevel aqi_recommendation;

  if(payload->status_code == AIRCHECK_STATUS_LOCATION_UNAVAILABLE) {
    return payload->has_live_data ? "Location unavailable" : "No cached data";
  }
  if(payload->status_code == AIRCHECK_STATUS_UPDATE_FAILED) {
    return payload->has_live_data ? "Update failed" : "No cached data";
  }
  if(payload->has_weather && is_dangerous_weather_code(payload->weather_code)) {
    return "Thunderstorm";
  }
  if(!payload->has_aqi) {
    temperature_reason = temperature_reason_for_payload(payload);
    uv_reason = uv_reason_for_payload(payload);

    if(temperature_reason && uv_reason &&
       temperature_recommendation_for_payload(payload) == RECOMMENDATION_CAUTION) {
      snprintf(reason_buffer, sizeof(reason_buffer), "%s + %s", temperature_reason, uv_reason);
      return reason_buffer;
    }
    if(temperature_reason) {
      return temperature_reason;
    }
    if(uv_reason) {
      return uv_reason;
    }
    if(payload->using_cache) {
      return "Cached reading";
    }
    return "Waiting for data";
  }
  aqi_recommendation = aqi_recommendation_for_payload(payload);
  aqi_reason = aqi_reason_for_payload(payload);
  temperature_reason = temperature_reason_for_payload(payload);
  uv_reason = uv_reason_for_payload(payload);

  if(aqi_recommendation == RECOMMENDATION_GO_OUT && temperature_reason && uv_reason &&
     temperature_recommendation_for_payload(payload) == RECOMMENDATION_CAUTION) {
    snprintf(reason_buffer, sizeof(reason_buffer), "%s + %s", temperature_reason, uv_reason);
    return reason_buffer;
  }
  if(aqi_recommendation == RECOMMENDATION_GO_OUT && uv_reason && !temperature_reason) {
    snprintf(reason_buffer, sizeof(reason_buffer), "%s + %s", aqi_reason, uv_reason);
    return reason_buffer;
  }
  if(aqi_reason && temperature_reason) {
    snprintf(reason_buffer, sizeof(reason_buffer), "%s + %s", aqi_reason, temperature_reason);
    return reason_buffer;
  }
  if(temperature_reason) {
    return temperature_reason;
  }
  if(aqi_reason) {
    return aqi_reason;
  }
  if(payload->using_cache) {
    return "Cached reading";
  }
  return "Waiting for data";
}

static const char *supporting_status_text(void) {
  if(s_live_payload.status_code == AIRCHECK_STATUS_LOCATION_UNAVAILABLE) {
    return s_live_payload.has_live_data ? "Location unavailable" : "No cached data";
  }
  if(s_live_payload.status_code == AIRCHECK_STATUS_UPDATE_FAILED) {
    return s_live_payload.has_live_data ? "Update failed" : "No cached data";
  }
  if(s_live_payload.using_cache) {
    return "Cached reading";
  }
  if(s_is_loading) {
    return "Refreshing...";
  }
  return "Waiting for data";
}

static const char *helper_text_for_payload(const LivePayload *payload) {
  (void)payload;

  if(s_manual_refresh_in_progress) {
    return "Refreshing...";
  }
  return "Select: Refresh";
}

static bool has_failure_status(const LivePayload *payload) {
  return payload->status_code == AIRCHECK_STATUS_LOCATION_UNAVAILABLE ||
         payload->status_code == AIRCHECK_STATUS_UPDATE_FAILED;
}

static void render_screen(void);

static void render_loading_state(void) {
  text_layer_set_text(s_title_layer, "AIRCHECK");
  text_layer_set_text_color(s_primary_layer, GColorWhite);
  text_layer_set_text(s_primary_layer, "Locating...");
  text_layer_set_text(s_secondary_layer, "Refreshing...");
  text_layer_set_text(s_detail_layer, s_live_payload.has_live_data ? "Showing cached data" : "Waiting for data");
  text_layer_set_text(s_reason_layer, "");
  text_layer_set_text(s_helper_layer, helper_text_for_payload(&s_live_payload));
}

static void render_summary_screen(const LivePayload *payload) {
  static char aqi_buffer[28];
  static char weather_buffer[28];

  text_layer_set_text(s_title_layer, "AIRCHECK");
  text_layer_set_text_color(s_primary_layer, verdict_color_for_payload(payload));
  text_layer_set_text(s_primary_layer, verdict_text_for_payload(payload));

  if(payload->has_aqi) {
    snprintf(aqi_buffer, sizeof(aqi_buffer), "AQI: %d", payload->aqi);
  } else {
    snprintf(aqi_buffer, sizeof(aqi_buffer), "AQI: --");
  }

  if(payload->has_weather) {
    snprintf(weather_buffer, sizeof(weather_buffer), "%d%s %s", payload->temperature,
             payload->use_celsius ? "C" : "F", weather_label_for_code(payload->weather_code));
  } else {
    snprintf(weather_buffer, sizeof(weather_buffer), "Weather unavailable");
  }

  text_layer_set_text(s_secondary_layer, aqi_buffer);
  text_layer_set_text(s_detail_layer, weather_buffer);
  text_layer_set_text(s_reason_layer, summary_reason_for_payload(payload));
  text_layer_set_text(s_helper_layer, helper_text_for_payload(payload));
}

static void render_aqi_screen(const LivePayload *payload) {
  static char aqi_buffer[16];

  text_layer_set_text(s_title_layer, "AIR QUALITY");
  text_layer_set_text_color(s_primary_layer, GColorWhite);

  if(payload->has_aqi) {
    snprintf(aqi_buffer, sizeof(aqi_buffer), "%d", payload->aqi);
    text_layer_set_text(s_primary_layer, aqi_buffer);
    text_layer_set_text(s_secondary_layer, aqi_category_for_value(payload->aqi));
    text_layer_set_text(s_detail_layer, aqi_guidance_for_value(payload->aqi));
    if(has_failure_status(payload)) {
      text_layer_set_text(s_reason_layer, supporting_status_text());
    } else {
      text_layer_set_text(s_reason_layer, payload->using_cache ? "Cached reading" : "");
    }
  } else {
    text_layer_set_text(s_primary_layer, "--");
    text_layer_set_text(s_secondary_layer, "NO DATA");
    text_layer_set_text(s_detail_layer, "Waiting for AQI");
    text_layer_set_text(s_reason_layer, supporting_status_text());
  }

  text_layer_set_text(s_helper_layer, helper_text_for_payload(payload));
}

static void render_weather_screen(const LivePayload *payload) {
  static char temp_buffer[16];
  static char uv_buffer[16];

  text_layer_set_text(s_title_layer, "WEATHER");
  text_layer_set_text_color(s_primary_layer, GColorWhite);

  if(payload->has_weather) {
    snprintf(temp_buffer, sizeof(temp_buffer), "%d%s", payload->temperature,
             payload->use_celsius ? "C" : "F");
    text_layer_set_text(s_primary_layer, temp_buffer);
    text_layer_set_text(s_secondary_layer, weather_label_for_code(payload->weather_code));
    if(has_failure_status(payload)) {
      text_layer_set_text(s_detail_layer, supporting_status_text());
      text_layer_set_text(s_reason_layer, payload->using_cache ? "Cached reading" : "");
    } else {
      if(payload->uv_index >= 0) {
        snprintf(uv_buffer, sizeof(uv_buffer), "UV: %d", payload->uv_index);
      } else {
        snprintf(uv_buffer, sizeof(uv_buffer), "UV: --");
      }
      text_layer_set_text(s_detail_layer,
                          is_dangerous_weather_code(payload->weather_code) ?
                          "Storm conditions" : uv_buffer);
      text_layer_set_text(s_reason_layer, payload->using_cache ? "Cached reading" : "");
    }
  } else {
    text_layer_set_text(s_primary_layer, "--");
    text_layer_set_text(s_secondary_layer, "WEATHER N/A");
    text_layer_set_text(s_detail_layer, "Waiting for weather");
    text_layer_set_text(s_reason_layer, supporting_status_text());
  }

  text_layer_set_text(s_helper_layer, helper_text_for_payload(payload));
}

static void render_screen(void) {
  if(s_is_loading && !s_live_payload.has_live_data) {
    render_loading_state();
    return;
  }

  switch(s_screen_index) {
    case 0:
      render_summary_screen(&s_live_payload);
      break;
    case 1:
      render_aqi_screen(&s_live_payload);
      break;
    case 2:
    default:
      render_weather_screen(&s_live_payload);
      break;
  }
}

static void request_refresh(bool manual_refresh) {
  DictionaryIterator *iterator;
  AppMessageResult result;

  s_manual_refresh_in_progress = manual_refresh;
  s_is_loading = true;
  s_live_payload.status_code = AIRCHECK_STATUS_LOADING;
  render_screen();

  result = app_message_outbox_begin(&iterator);
  if(result != APP_MSG_OK) {
    s_manual_refresh_in_progress = false;
    s_is_loading = false;
    s_live_payload.status_code = AIRCHECK_STATUS_UPDATE_FAILED;
    render_screen();
    return;
  }

  dict_write_uint8(iterator, MESSAGE_KEY_RequestRefresh, 1);
  dict_write_end(iterator);
  result = app_message_outbox_send();
  if(result != APP_MSG_OK) {
    s_manual_refresh_in_progress = false;
    s_is_loading = false;
    s_live_payload.status_code = AIRCHECK_STATUS_UPDATE_FAILED;
    render_screen();
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *status_tuple = dict_find(iterator, MESSAGE_KEY_StatusCode);
  Tuple *flags_tuple = dict_find(iterator, MESSAGE_KEY_Flags);
  Tuple *aqi_tuple = dict_find(iterator, MESSAGE_KEY_AQI);
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_Temperature);
  Tuple *temperature_unit_tuple = dict_find(iterator, MESSAGE_KEY_TemperatureUnit);
  Tuple *weather_tuple = dict_find(iterator, MESSAGE_KEY_WeatherCode);
  Tuple *uv_tuple = dict_find(iterator, MESSAGE_KEY_UvIndex);
  Tuple *location_mode_tuple = dict_find(iterator, MESSAGE_KEY_LocationMode);
  Tuple *fixed_zip_tuple = dict_find(iterator, MESSAGE_KEY_FixedZip);

  if(status_tuple) {
    s_live_payload.status_code = status_tuple->value->int32;
  }

  if(flags_tuple) {
    int flags = flags_tuple->value->int32;
    s_live_payload.has_aqi = (flags & FLAG_HAS_AQI) != 0;
    s_live_payload.has_weather = (flags & FLAG_HAS_WEATHER) != 0;
    s_live_payload.using_cache = (flags & FLAG_USING_CACHE) != 0;
  }

  if(aqi_tuple) {
    s_live_payload.aqi = aqi_tuple->value->int32;
  }
  if(temp_tuple) {
    s_live_payload.temperature = temp_tuple->value->int32;
  }
  if(temperature_unit_tuple) {
  s_live_payload.use_celsius = temperature_unit_tuple->value->int32 == 1;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Temp unit received: %d", s_live_payload.use_celsius);
}
  if(weather_tuple) {
    s_live_payload.weather_code = weather_tuple->value->int32;
  }
  if(uv_tuple) {
    s_live_payload.uv_index = uv_tuple->value->int32;
  }
  if(location_mode_tuple && location_mode_tuple->value->cstring[0] != '\0') {
    s_live_payload.is_fixed_location = strcmp(location_mode_tuple->value->cstring, "fixed") == 0;
  }
  if(fixed_zip_tuple && fixed_zip_tuple->value->cstring[0] != '\0') {
    snprintf(s_live_payload.fixed_zip, sizeof(s_live_payload.fixed_zip), "%s",
             fixed_zip_tuple->value->cstring);
  }

  s_live_payload.has_live_data = s_live_payload.has_aqi || s_live_payload.has_weather;
  s_manual_refresh_in_progress = false;
  s_is_loading = (s_live_payload.status_code == AIRCHECK_STATUS_LOADING);
  render_screen();
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Inbox dropped: %d", reason);
  s_manual_refresh_in_progress = false;
  render_screen();
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason,
                                   void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Outbox failed: %d", reason);
  s_manual_refresh_in_progress = false;
  s_is_loading = false;
  s_live_payload.status_code = AIRCHECK_STATUS_UPDATE_FAILED;
  render_screen();
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_screen_index = (s_screen_index + SCREEN_COUNT - 1) % SCREEN_COUNT;
  render_screen();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  s_screen_index = (s_screen_index + 1) % SCREEN_COUNT;
  render_screen();
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  request_refresh(true);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  bool is_round = PBL_IF_ROUND_ELSE(true, false);
  bool is_bw;
  GFont primary_font;
  GFont secondary_font;
  int horizontal_inset = is_round ? 12 : 4;
  int title_y = is_round ? 10 : 2;
  int primary_y = is_round ? 30 : 20;
  int primary_h = is_round ? 44 : 40;
  int secondary_y = is_round ? 76 : 64;
  int secondary_h;
  int detail_y = is_round ? 104 : 94;
  int detail_h;
  int reason_y = is_round ? 128 : 120;
  int reason_h = is_round ? 32 : 28;
  int helper_y = bounds.size.h - (is_round ? 24 : 16);
  GRect content_bounds = inset_rect(bounds, horizontal_inset);

#ifdef PBL_BW
  is_bw = true;
#else
  is_bw = false;
#endif

  primary_font = is_round ? fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK) :
                            fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  secondary_font = is_bw ? fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD) :
                           fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

  secondary_h = is_bw ? 28 : 28;
  detail_h = is_bw ? 22 : 24;

  s_title_layer = create_text_layer(GRect(content_bounds.origin.x, title_y,
                                          content_bounds.size.w, 18),
                                    fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                                    GTextAlignmentCenter);
  s_primary_layer = create_text_layer(GRect(content_bounds.origin.x, primary_y,
                                            content_bounds.size.w, primary_h),
                                      primary_font,
                                      GTextAlignmentCenter);
  s_secondary_layer = create_text_layer(GRect(content_bounds.origin.x, secondary_y,
                                              content_bounds.size.w, secondary_h),
                                        secondary_font,
                                        GTextAlignmentCenter);
  s_detail_layer = create_text_layer(GRect(content_bounds.origin.x, detail_y,
                                           content_bounds.size.w, detail_h),
                                     fonts_get_system_font(FONT_KEY_GOTHIC_18),
                                     GTextAlignmentCenter);
  s_reason_layer = create_text_layer(GRect(content_bounds.origin.x + 2, reason_y,
                                           content_bounds.size.w - 4, reason_h),
                                     fonts_get_system_font(FONT_KEY_GOTHIC_18),
                                     GTextAlignmentCenter);
  s_helper_layer = create_text_layer(GRect(content_bounds.origin.x, helper_y,
                                           content_bounds.size.w, 14),
                                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                                     GTextAlignmentCenter);

  layer_add_child(window_layer, text_layer_get_layer(s_title_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_primary_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_secondary_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_detail_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_reason_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_helper_layer));

  render_screen();
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_primary_layer);
  text_layer_destroy(s_secondary_layer);
  text_layer_destroy(s_detail_layer);
  text_layer_destroy(s_reason_layer);
  text_layer_destroy(s_helper_layer);
}

static void init(void) {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_click_config_provider(s_main_window, click_config_provider);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_open(128, 128);

  window_stack_push(s_main_window, true);
  request_refresh(false);
}

static void deinit(void) {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
