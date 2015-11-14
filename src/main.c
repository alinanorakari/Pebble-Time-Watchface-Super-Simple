#include <pebble.h>

#define MINUTE_COLOR       GColorWhite
#define HOUR_COLOR         GColorRed
#define PEG_COLOR          GColorDarkGray
#define BG_COLOR           GColorBlack


#define ANTIALIASING true

#define FINAL_RADIUS       88
#define HAND_WIDTH         7
#define DOT_RADIUS         HAND_WIDTH/4
#define HAND_MARGIN_OUTER  10-(HAND_WIDTH/2)
#define HAND_MARGIN_INNER  0

#define ANIMATION_DURATION 600
#define ANIMATION_DELAY    150

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
  graphics_context_set_fill_color(ctx, BG_COLOR);
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
  if((s_radius - HAND_MARGIN_OUTER) > HAND_MARGIN_INNER) {
    if(s_radius > 2 * HAND_MARGIN_OUTER) {
      graphics_context_set_stroke_color(ctx, HOUR_COLOR);
      graphics_context_set_stroke_width(ctx, HAND_WIDTH);
      graphics_draw_line(ctx, hour_hand_inner, hour_hand_outer);
    } 
    if(s_radius > HAND_MARGIN_OUTER) {
      graphics_context_set_stroke_color(ctx, MINUTE_COLOR);
      graphics_context_set_stroke_width(ctx, HAND_WIDTH);
      graphics_draw_line(ctx, minute_hand_inner, minute_hand_outer);
    }
  }
  graphics_context_set_fill_color(ctx, PEG_COLOR);
  graphics_fill_circle(ctx, s_center, DOT_RADIUS);

}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_center = grect_center_point(&window_bounds);

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


