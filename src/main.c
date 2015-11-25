#include <pebble.h>

#define KEY_BG_COLOR           0
#define KEY_MINUTE_COLOR       1
#define KEY_HOUR_COLOR         2
#define KEY_PEG_COLOR          3
#define KEY_SHADOWS            4
#define KEY_TICKS              5
#define KEY_TICK_COLOR         6
#define KEY_RECT_TICKS         7

#define ANTIALIASING true

#define HAND_WIDTH             7
#define TICK_RADIUS            3
#define DOT_RADIUS             HAND_WIDTH/4
#define HAND_MARGIN_M          16
#define HAND_MARGIN_H          42
#define SHADOW_OFFSET          2

#define ANIMATION_DURATION     750
#define ANIMATION_DELAY        0

static uint8_t shadowtable[] = {192,192,192,192,192,192,192,192,192,192,192,192,192,192,192,192, \
                                192,192,192,192,192,192,192,192,192,192,192,192,192,192,192,192, \
                                192,192,192,192,192,192,192,192,192,192,192,192,192,192,192,192, \
                                192,192,192,192,192,192,192,192,192,192,192,192,192,192,192,192, \
                                192,192,192,193,192,192,192,193,192,192,192,193,196,196,196,197, \
                                192,192,192,193,192,192,192,193,192,192,192,193,196,196,196,197, \
                                192,192,192,193,192,192,192,193,192,192,192,193,196,196,196,197, \
                                208,208,208,209,208,208,208,209,208,208,208,209,212,212,212,213, \
                                192,192,193,194,192,192,193,194,196,196,197,198,200,200,201,202, \
                                192,192,193,194,192,192,193,194,196,196,197,198,200,200,201,202, \
                                208,208,209,210,208,208,209,210,212,212,213,214,216,216,217,218, \
                                224,224,225,226,224,224,225,226,228,228,229,230,232,232,233,234, \
                                192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207, \
                                208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223, \
                                224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239, \
                                240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255};

// alpha should only be 0b??111111 where ?? = 00 (full shade), 01 (much shade), 10 (some shade), 11 (none shade)
static uint8_t alpha = 0b10111111;

typedef struct {
  int hours;
  int minutes;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer;

static GPoint s_center;
static Time s_last_time;
static int animpercent = 0, ticks;
static bool s_animating = false, shadows = true, debug = false, rectticks = true;

static GColor gcolorbg, gcolorm, gcolorh, gcolorp, gcolorshadow, gcolort;

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *colorbg_t = dict_find(iter, KEY_BG_COLOR);
  Tuple *colorm_t = dict_find(iter, KEY_MINUTE_COLOR);
  Tuple *colorh_t = dict_find(iter, KEY_HOUR_COLOR);
  Tuple *colorp_t = dict_find(iter, KEY_PEG_COLOR);
  Tuple *shadows_t = dict_find(iter, KEY_SHADOWS);
  Tuple *ticknum_t = dict_find(iter, KEY_TICKS);
  Tuple *colort_t = dict_find(iter, KEY_TICK_COLOR);
  Tuple *rectticks_t = dict_find(iter, KEY_RECT_TICKS);
    
  if(colorbg_t) {
    int colorbg = colorbg_t->value->int32;
    persist_write_int(KEY_BG_COLOR, colorbg);
    gcolorbg = GColorFromHEX(colorbg);
    gcolorshadow = (GColor8) shadowtable[alpha & gcolorbg.argb];
  }
  if(colorm_t) {
    int colorm = colorm_t->value->int32;
    persist_write_int(KEY_MINUTE_COLOR, colorm);
    gcolorm = GColorFromHEX(colorm);
  }
  if(colorh_t) {
    int colorh = colorh_t->value->int32;
    persist_write_int(KEY_HOUR_COLOR, colorh);
    gcolorh = GColorFromHEX(colorh);
  }
  if(colorp_t) {
    int colorp = colorp_t->value->int32;
    persist_write_int(KEY_PEG_COLOR, colorp);
    gcolorp = GColorFromHEX(colorp);
  }
  if(shadows_t && shadows_t->value->int8 > 0) {
    persist_write_bool(KEY_SHADOWS, true);
    shadows = true;
  } else {
    persist_write_bool(KEY_SHADOWS, false);
    shadows = false;
  }
  if(ticknum_t) {
    ticks = ticknum_t->value->uint8;
    persist_write_int(KEY_TICKS, ticks);
  }
  if(colort_t) {
    int colort = colort_t->value->int32;
    persist_write_int(KEY_TICK_COLOR, colort);
    gcolort = GColorFromHEX(colort);
  }
  if(rectticks_t && rectticks_t->value->int8 > 0) {
    persist_write_bool(KEY_RECT_TICKS, true);
    rectticks = true;
  } else {
    persist_write_bool(KEY_RECT_TICKS, false);
    rectticks = false;
  }
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

/*************************** AnimationImplementation **************************/

static void animation_started(Animation *anim, void *context) {
  s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  s_animating = false;
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
  Animation *anim = animation_create();
  animation_set_duration(anim, duration);
  animation_set_delay(anim, delay);
  animation_set_curve(anim, AnimationCurveEaseInOut);
  animation_set_implementation(anim, implementation);
  if(handlers) {
    animation_set_handlers(anim, (AnimationHandlers) {
      .started = animation_started,
      .stopped = animation_stopped
    }, NULL);
  }
  animation_schedule(anim);
}

/************************************ UI **************************************/

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  // Store time
  // dummy time in emulator
  if (watch_info_get_model()==WATCH_INFO_MODEL_UNKNOWN) {
    s_last_time.hours = 0;
    s_last_time.minutes = tick_time->tm_sec;
  } else {
    s_last_time.hours = tick_time->tm_hour;
    s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
    s_last_time.minutes = tick_time->tm_min;
  }

  // Redraw
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static int32_t get_angle_for_minute(int minute) {
  // Progress through 60 minutes, out of 360 degrees
  return ((minute * 360) / 60);
}

static int32_t get_angle_for_hour(int hour, int minute) {
  // Progress through 12 hours, out of 360 degrees
  return (((hour * 360) / 12)+(get_angle_for_minute(minute)/12));
}

static void update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GRect bounds_h = bounds;
  bounds_h.size.w = bounds_h.size.h;
  bounds_h.origin.x -= (bounds_h.size.w-bounds.size.w)/2;
  int maxradius = bounds_h.size.w;
  if (bounds_h.size.h < maxradius) { maxradius = bounds_h.size.h; }
  maxradius /= 2;
  int animradius = maxradius-((maxradius*animpercent)/100);
  #if defined(PBL_RECT)
    int platform_margin_m = (HAND_MARGIN_M/1.5);
  #elif defined(PBL_ROUND)
    int platform_margin_m = HAND_MARGIN_M;
  #endif
  int outer_m = animradius+platform_margin_m;
  int outer_h = animradius+HAND_MARGIN_H;

  if (outer_m < platform_margin_m) {
    outer_m = platform_margin_m;
  }
  if (outer_h < HAND_MARGIN_H) {
    outer_h = HAND_MARGIN_H;
  }
  if (outer_m > maxradius) {
    outer_m = maxradius;
  }
  if (outer_h > maxradius) {
    outer_h = maxradius;
  }
  GRect bounds_mo = grect_inset(bounds_h, GEdgeInsets(outer_m));
  GRect bounds_ho = grect_inset(bounds_h, GEdgeInsets(outer_h));
  graphics_context_set_fill_color(ctx, gcolorbg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_antialiased(ctx, ANTIALIASING);

  // Use current time while animating
  Time mode_time = s_last_time;

  // Adjust for minutes through the hour
  float hour_deg = get_angle_for_hour(mode_time.hours, mode_time.minutes);
  float minute_deg = get_angle_for_minute(mode_time.minutes);

  GPoint minute_hand_outer = gpoint_from_polar(bounds_mo, GOvalScaleModeFillCircle, DEG_TO_TRIGANGLE(minute_deg));
  GPoint hour_hand_outer = gpoint_from_polar(bounds_ho, GOvalScaleModeFillCircle, DEG_TO_TRIGANGLE(hour_deg));
  
  if (ticks > 0) {
    graphics_context_set_fill_color(ctx, gcolort);
    #if defined(PBL_RECT)
      if (rectticks) {
        int dist_v = 41;
        int dist_h = 46;
        switch (ticks) {
          case 12:
            graphics_fill_circle(ctx, GPoint((bounds.size.w/2)+dist_h-1, 4), TICK_RADIUS/2); // 1
            graphics_fill_circle(ctx, GPoint((bounds.size.w/2)-dist_h-1, 4), TICK_RADIUS/2); // 11
            graphics_fill_circle(ctx, GPoint((bounds.size.w/2)+dist_h-1, (bounds.size.h-5)), TICK_RADIUS/2); // 5
            graphics_fill_circle(ctx, GPoint((bounds.size.w/2)-dist_h-1, (bounds.size.h-5)), TICK_RADIUS/2); // 7
            graphics_fill_circle(ctx, GPoint((bounds.size.w-5), (bounds.size.h/2)-dist_v-1), TICK_RADIUS/2); // 2
            graphics_fill_circle(ctx, GPoint((bounds.size.w-5), (bounds.size.h/2)+dist_v-1), TICK_RADIUS/2); // 4
            graphics_fill_circle(ctx, GPoint(4, (bounds.size.h/2)-dist_v-1), TICK_RADIUS/2); // 10
            graphics_fill_circle(ctx, GPoint(4, (bounds.size.h/2)+dist_v-1), TICK_RADIUS/2); // 8
          case 4:
            graphics_fill_circle(ctx, GPoint((bounds.size.w/2)-1, (bounds.size.h-3)), TICK_RADIUS); // 6
            graphics_fill_circle(ctx, GPoint((bounds.size.w-3), (bounds.size.h/2)-1), TICK_RADIUS); // 3
            graphics_fill_circle(ctx, GPoint(2, (bounds.size.h/2)-1), TICK_RADIUS); // 9
          case 1:
          default:
            graphics_fill_circle(ctx, GPoint((bounds.size.w/2)-1, 2), TICK_RADIUS); // 12
            break;
        }
      } else {
    #endif
    GRect insetbounds = grect_inset(bounds, GEdgeInsets(2));
    GRect insetbounds12 = grect_inset(bounds, GEdgeInsets(4));
    for(int i = 0; i < ticks; i++) {
      int hour_angle = (i * 360) / ticks;
      if (ticks == 12 && i%3!=0) {
        GPoint pos = gpoint_from_polar(insetbounds12, GOvalScaleModeFitCircle , DEG_TO_TRIGANGLE(hour_angle));
        graphics_fill_circle(ctx, pos, TICK_RADIUS/2);
      } else {
        GPoint pos = gpoint_from_polar(insetbounds, GOvalScaleModeFitCircle , DEG_TO_TRIGANGLE(hour_angle));
        graphics_fill_circle(ctx, pos, TICK_RADIUS);
      }
    }
    #if defined(PBL_RECT)
      }
    #endif
  }

  if(shadows) {
    graphics_context_set_stroke_color(ctx, gcolorshadow);
    graphics_context_set_stroke_width(ctx, HAND_WIDTH);
    hour_hand_outer.y += SHADOW_OFFSET;
    s_center.y += SHADOW_OFFSET;
    graphics_draw_line(ctx, s_center, hour_hand_outer);
    minute_hand_outer.y += SHADOW_OFFSET+1;
    s_center.y += 1;
    graphics_draw_line(ctx, s_center, minute_hand_outer);
    hour_hand_outer.y -= SHADOW_OFFSET;
    minute_hand_outer.y -= SHADOW_OFFSET+1;
    s_center.y -= SHADOW_OFFSET+1;
  }
  graphics_context_set_stroke_color(ctx, gcolorh);
  graphics_context_set_stroke_width(ctx, HAND_WIDTH);
  graphics_draw_line(ctx, s_center, hour_hand_outer);
  graphics_context_set_stroke_color(ctx, gcolorm);
  graphics_context_set_stroke_width(ctx, HAND_WIDTH);
  graphics_draw_line(ctx, s_center, minute_hand_outer);
  graphics_context_set_fill_color(ctx, gcolorp);
  graphics_fill_circle(ctx, s_center, DOT_RADIUS);

}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_center = grect_center_point(&window_bounds);
  s_center.x -= 1;
  s_center.y -= 1;
    
  if (persist_exists(KEY_BG_COLOR)) {
    int colorbg = persist_read_int(KEY_BG_COLOR);
    gcolorbg = GColorFromHEX(colorbg);
  } else {
    gcolorbg = GColorBlack;
  }
  gcolorshadow = (GColor8) shadowtable[alpha & gcolorbg.argb];
  if (persist_exists(KEY_MINUTE_COLOR)) {
    int colorm = persist_read_int(KEY_MINUTE_COLOR);
    gcolorm = GColorFromHEX(colorm);
  } else {
    gcolorm = GColorWhite;
  }
  if (persist_exists(KEY_HOUR_COLOR)) {
    int colorh = persist_read_int(KEY_HOUR_COLOR);
    gcolorh = GColorFromHEX(colorh);
  } else {
    gcolorh = GColorRed;
  }
  if (persist_exists(KEY_PEG_COLOR)) {
    int colorp = persist_read_int(KEY_PEG_COLOR);
    gcolorp = GColorFromHEX(colorp);
  } else {
    gcolorp = GColorDarkGray;
  }
  if (persist_exists(KEY_SHADOWS)) {
    shadows = persist_read_bool(KEY_SHADOWS);
  } else {
    shadows = false;
  }
  if (persist_exists(KEY_TICKS)) {
    ticks = persist_read_int(KEY_TICKS);
  } else {
    ticks = 0;
  }
  if (persist_exists(KEY_TICK_COLOR)) {
    int colort = persist_read_int(KEY_TICK_COLOR);
    gcolort = GColorFromHEX(colort);
  } else {
    gcolort = GColorWhite;
  }
  if (persist_exists(KEY_RECT_TICKS)) {
    rectticks = persist_read_bool(KEY_RECT_TICKS);
  } else {
    rectticks = false;
  }

  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, s_canvas_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

/*********************************** App **************************************/

static int anim_percentage(AnimationProgress dist_normalized, int max) {
  return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}

static void radius_update(Animation *anim, AnimationProgress dist_normalized) {
  animpercent = anim_percentage(dist_normalized, 100);
  layer_mark_dirty(s_canvas_layer);
}

static void init() {
  srand(time(NULL));
  
  // keep lit only in emulator
  if (watch_info_get_model()==WATCH_INFO_MODEL_UNKNOWN) {
    debug = true;
  }

  time_t t = time(NULL);
  struct tm *time_now = localtime(&t);
  if (debug) {
    tick_handler(time_now, SECOND_UNIT);
  } else {
    tick_handler(time_now, MINUTE_UNIT);
  }

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_main_window, true);

  if (debug) {
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  }
  
  if (debug) {
    light_enable(true);
  }
    
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  // Prepare animations
  AnimationImplementation radius_impl = {
    .update = radius_update
  };
  animate(ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}

