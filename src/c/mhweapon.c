#include "gbitmap_color_palette_manipulator.h"

static Window *s_window;
static TextLayer *s_time_layer,
                 *s_date_layer,
                 *s_step_layer;
static BitmapLayer *s_background_layer;
static GBitmap *s_bitmap_weapon;
static BitmapLayer *s_battery_layer;
static GBitmap *s_bitmap_battery;
static Layer *s_batt_layer;

static int s_battery_level;
static int s_step_count;

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_buffer);
}

static void update_date() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  static char s_buffer[16];
  strftime(s_buffer, sizeof(s_buffer), "%B %e", tick_time);
  text_layer_set_text(s_date_layer, s_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  if (units_changed & DAY_UNIT) {
    update_date();
  }
}

static void batt_update_proc(Layer *layer, GContext *ctx) {
  GRect red1 = GRect(PBL_IF_ROUND_ELSE(54, 36), PBL_IF_ROUND_ELSE(23, 10), 8, 8);
  GRect red2 = GRect(PBL_IF_ROUND_ELSE(62, 44), PBL_IF_ROUND_ELSE(23, 10), 8, 8);
  GRect orange1 = GRect(PBL_IF_ROUND_ELSE(70, 52), PBL_IF_ROUND_ELSE(23, 10), 8, 8);
  GRect orange2 = GRect(PBL_IF_ROUND_ELSE(78, 60), PBL_IF_ROUND_ELSE(23, 10), 8, 8);
  GRect yellow1 = GRect(PBL_IF_ROUND_ELSE(86, 68), PBL_IF_ROUND_ELSE(23, 10), 8, 8);
  GRect yellow2 = GRect(PBL_IF_ROUND_ELSE(94, 76), PBL_IF_ROUND_ELSE(23, 10), 8, 8);
  GRect green1 = GRect(PBL_IF_ROUND_ELSE(102, 84), PBL_IF_ROUND_ELSE(23, 10), 8, 8);
  GRect green2 = GRect(PBL_IF_ROUND_ELSE(110, 92), PBL_IF_ROUND_ELSE(23, 10), 8, 8);
  GRect blue = GRect(PBL_IF_ROUND_ELSE(118, 100), PBL_IF_ROUND_ELSE(23, 10), 8, 8);
  GRect white = GRect(PBL_IF_ROUND_ELSE(126, 108), PBL_IF_ROUND_ELSE(23, 10), 4, 8);

  if (s_battery_level > 0) {
    graphics_context_set_fill_color(ctx, PBL_IF_BW_ELSE(GColorWhite, GColorRed));
    graphics_fill_rect(ctx, red1, 0, GCornersAll);
    if (s_battery_level > 10) {
      graphics_fill_rect(ctx, red2, 0, GCornersAll);
      if (s_battery_level > 20) {
        graphics_context_set_fill_color(ctx, GColorRajah);
        graphics_fill_rect(ctx, orange1, 0, GCornersAll);
        if (s_battery_level > 30) {
          graphics_fill_rect(ctx, orange2, 0, GCornersAll);
          if (s_battery_level > 40) {
            graphics_context_set_fill_color(ctx, GColorYellow);
            graphics_fill_rect(ctx, yellow1, 0, GCornersAll);
            if (s_battery_level > 50) {
              graphics_fill_rect(ctx, yellow2, 0, GCornersAll);
              if (s_battery_level > 60) {
                graphics_context_set_fill_color(ctx, GColorGreen);
                graphics_fill_rect(ctx, green1, 0, GCornersAll);
                if (s_battery_level > 70) {
                  graphics_fill_rect(ctx, green2, 0, GCornersAll);
                  if (s_battery_level > 80) {
                    graphics_context_set_fill_color(ctx, PBL_IF_BW_ELSE(GColorWhite, GColorLiberty));
                    graphics_fill_rect(ctx, blue, 0, GCornersAll);
                    if (s_battery_level > 90) {
                      graphics_context_set_fill_color(ctx, GColorWhite);
                      graphics_fill_rect(ctx, white, 0, GCornersAll);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

static void battery_callback(BatteryChargeState state) {
  s_battery_level = state.charge_percent;
  layer_mark_dirty(s_batt_layer);
}

static void bluetooth_callback(bool connected) {
  if (!connected) {   
    static const uint32_t const segments[] = { 300, 300, 300, 300, 300 };
    VibePattern pat = {
      .durations = segments,
      .num_segments = ARRAY_LENGTH(segments),
    };
    vibes_enqueue_custom_pattern(pat);
  }
}

static void update_steps() {
  HealthMetric metric = HealthMetricStepCount;
  time_t start = time_start_of_today();
  time_t end = time(NULL);

  HealthServiceAccessibilityMask mask = health_service_metric_accessible(metric, 
    start, end);

  if(mask & HealthServiceAccessibilityMaskAvailable) {
    s_step_count = (int)health_service_sum_today(metric);
    APP_LOG(APP_LOG_LEVEL_INFO, "Steps today: %d", s_step_count);
    static char s_buffer[32];
    snprintf(s_buffer, sizeof(s_buffer), "%d steps", s_step_count);    
    text_layer_set_text(s_step_layer, s_buffer);
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Data unavailable!");
  }
}

static void health_callback(HealthEventType event, void *context) {
  switch(event) {
    case HealthEventSignificantUpdate:
      APP_LOG(APP_LOG_LEVEL_INFO, 
              "New HealthService HealthEventSignificantUpdate event");
      break;
    case HealthEventMovementUpdate:
      APP_LOG(APP_LOG_LEVEL_INFO, 
              "New HealthService HealthEventMovementUpdate event");
      update_steps();
      break;
    case HealthEventSleepUpdate:
      APP_LOG(APP_LOG_LEVEL_INFO, 
              "New HealthService HealthEventSleepUpdate event");
      break;
    case HealthEventHeartRateUpdate:
      APP_LOG(APP_LOG_LEVEL_INFO,
              "New HealthService HealthEventHeartRateUpdate event");
      break;
    default:
      break;
  }
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_background_layer = bitmap_layer_create(GRect(0, PBL_IF_ROUND_ELSE(40, 27), bounds.size.w, 65));
  bitmap_layer_set_compositing_mode(s_background_layer, GCompOpSet);
  s_bitmap_weapon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_HUNTINGHORN);
  #ifdef PBL_COLOR
  bitmap_layer_set_bitmap(s_background_layer, s_bitmap_weapon);
  spit_gbitmap_color_palette(s_bitmap_weapon);
  if (gbitmap_color_palette_contains_color(GColorWhite, s_bitmap_weapon)) {
    replace_gbitmap_color(GColorWhite, GColorRed, s_bitmap_weapon, s_background_layer);
  }
  #endif
  layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));

  s_time_layer = text_layer_create(GRect(0, bounds.size.h-80, bounds.size.w, 40));
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  update_time();
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  s_date_layer = text_layer_create(GRect(0, bounds.size.h-56, bounds.size.w, 35));
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  update_date();
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  s_step_layer = text_layer_create(GRect(0, bounds.size.h-30, bounds.size.w, 25));
  text_layer_set_font(s_step_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_step_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_step_layer, GColorClear);
  text_layer_set_text_color(s_step_layer, GColorBlack);
  update_steps();
  layer_add_child(window_layer, text_layer_get_layer(s_step_layer));

  s_battery_layer = bitmap_layer_create(GRect(0, PBL_IF_ROUND_ELSE(15, 2), bounds.size.w, 25));
  bitmap_layer_set_compositing_mode(s_battery_layer, GCompOpSet);
  bitmap_layer_set_alignment(s_battery_layer, GAlignCenter);
  s_bitmap_battery = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SHARP000);
  bitmap_layer_set_bitmap(s_battery_layer, s_bitmap_battery);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_battery_layer));

  s_batt_layer = layer_create(bounds);
  layer_set_update_proc(s_batt_layer, batt_update_proc);
  battery_callback(battery_state_service_peek());
  layer_add_child(window_layer, s_batt_layer);

  bluetooth_callback(connection_service_peek_pebble_app_connection());
}

static void prv_window_unload(Window *window) {
  gbitmap_destroy(s_bitmap_weapon);
  gbitmap_destroy(s_bitmap_battery);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_step_layer);
  bitmap_layer_destroy(s_background_layer);
  bitmap_layer_destroy(s_battery_layer);
}

static void prv_init(void) {
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_callback);
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_callback
  });
  
  #if defined(PBL_HEALTH)
  // Attempt to subscribe 
  if(!health_service_events_subscribe(health_callback, NULL)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Health not available!");
  }
  #else
  APP_LOG(APP_LOG_LEVEL_ERROR, "Health not available!");
  #endif

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) {
  window_destroy(s_window);
  tick_timer_service_unsubscribe();
  connection_service_unsubscribe();
  battery_state_service_unsubscribe();
  health_service_events_unsubscribe();
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}
