#include <pebble.h>

#define KEY_BG_COLOR           0
#define KEY_MINUTE_COLOR       1
#define KEY_HOUR_COLOR         2
#define KEY_PEG_COLOR          3


#define ANTIALIASING true

#define FINAL_RADIUS       88
#define HAND_WIDTH         7
#define DOT_RADIUS         HAND_WIDTH/4
#define HAND_MARGIN_OUTER  10-(HAND_WIDTH/2)
#define HAND_MARGIN_INNER  0
#define SHADOW_OFFSET      2

#define ANIMATION_DURATION 600
#define ANIMATION_DELAY    150

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
static int s_radius = 0;
static bool s_animating = false;
static float anim_offset;

static GColor gcolorbg, gcolorm, gcolorh, gcolorp, gcolorshadow;

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *colorbg_t = dict_find(iter, KEY_BG_COLOR);
  Tuple *colorm_t = dict_find(iter, KEY_MINUTE_COLOR);
  Tuple *colorh_t = dict_find(iter, KEY_HOUR_COLOR);
  Tuple *colorp_t = dict_find(iter, KEY_PEG_COLOR);
    
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
  s_last_time.hours = tick_time->tm_hour;
  s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
  s_last_time.minutes = tick_time->tm_min;

  // Redraw
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void update_proc(Layer *layer, GContext *ctx) {
  // Color background?
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, gcolorbg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_antialiased(ctx, ANTIALIASING);

  // Don't use current time while animating
  Time mode_time = s_last_time;

  // Adjust for minutes through the hour
  float hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
  float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
  hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);
  if (s_animating) {
	  hour_angle += anim_offset;
	  minute_angle -= anim_offset;
  }

  // Plot hands
  GPoint minute_hand_outer = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN_OUTER) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN_OUTER) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint minute_hand_inner = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)HAND_MARGIN_INNER / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)HAND_MARGIN_INNER / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint hour_hand_outer = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)(s_radius - HAND_MARGIN_OUTER - (0.3 * s_radius)) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)(s_radius - HAND_MARGIN_OUTER - (0.3 * s_radius)) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint hour_hand_inner = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)HAND_MARGIN_INNER / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)HAND_MARGIN_INNER / TRIG_MAX_RATIO) + s_center.y,
  };
  // Draw hands with positive length only
  //gcolorshadow gcolorh gcolorm
  if((s_radius - HAND_MARGIN_OUTER) > HAND_MARGIN_INNER) {
    if(s_radius > 2 * HAND_MARGIN_OUTER) {
      graphics_context_set_stroke_color(ctx, gcolorshadow);
      graphics_context_set_stroke_width(ctx, HAND_WIDTH);
      hour_hand_inner.y += SHADOW_OFFSET; hour_hand_outer.y += SHADOW_OFFSET;
      graphics_draw_line(ctx, hour_hand_inner, hour_hand_outer);
      hour_hand_inner.y -= SHADOW_OFFSET; hour_hand_outer.y -= SHADOW_OFFSET;
    }
    if(s_radius > HAND_MARGIN_OUTER) {
      graphics_context_set_stroke_color(ctx, gcolorshadow);
      graphics_context_set_stroke_width(ctx, HAND_WIDTH);
      minute_hand_inner.y += SHADOW_OFFSET+1; minute_hand_outer.y += SHADOW_OFFSET+1;
      graphics_draw_line(ctx, minute_hand_inner, minute_hand_outer);
      minute_hand_inner.y -= SHADOW_OFFSET+1; minute_hand_outer.y -= SHADOW_OFFSET+1;
    }
    if(s_radius > 2 * HAND_MARGIN_OUTER) {
      graphics_context_set_stroke_color(ctx, gcolorh);
      graphics_context_set_stroke_width(ctx, HAND_WIDTH);
      graphics_draw_line(ctx, hour_hand_inner, hour_hand_outer);
    }
    if(s_radius > HAND_MARGIN_OUTER) {
      graphics_context_set_stroke_color(ctx, gcolorm);
      graphics_context_set_stroke_width(ctx, HAND_WIDTH);
      graphics_draw_line(ctx, minute_hand_inner, minute_hand_outer);
    }
  }
  graphics_context_set_fill_color(ctx, gcolorp);
  graphics_fill_circle(ctx, s_center, DOT_RADIUS);

}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_center = grect_center_point(&window_bounds);
    
  if (persist_exists(KEY_BG_COLOR)) {
    int colorbg = persist_read_int(KEY_BG_COLOR);
    gcolorbg = GColorFromHEX(colorbg);
  } else {
    gcolorbg=GColorBlack;
  }
  gcolorshadow = (GColor8) shadowtable[alpha & gcolorbg.argb];
  if (persist_exists(KEY_MINUTE_COLOR)) {
    int colorm = persist_read_int(KEY_MINUTE_COLOR);
    gcolorm = GColorFromHEX(colorm);
  } else {
    gcolorm=GColorWhite;
  }
  if (persist_exists(KEY_HOUR_COLOR)) {
    int colorh = persist_read_int(KEY_HOUR_COLOR);
    gcolorh = GColorFromHEX(colorh);
  } else {
    gcolorh=GColorRed;
  }
  if (persist_exists(KEY_PEG_COLOR)) {
    int colorp = persist_read_int(KEY_PEG_COLOR);
    gcolorp = GColorFromHEX(colorp);
  } else {
    gcolorp=GColorDarkGray;
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
  s_radius = anim_percentage(dist_normalized, FINAL_RADIUS);
  layer_mark_dirty(s_canvas_layer);
}

static void init() {
  srand(time(NULL));

  time_t t = time(NULL);
  struct tm *time_now = localtime(&t);
  tick_handler(time_now, MINUTE_UNIT);

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    
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

