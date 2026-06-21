#include "ui_loading.h"
#include "theme.h"

#include <pebble.h>

/*
 * Repebble graphics guide: use Animation (custom implementation) for smooth
 * progress + drawing primitives (graphics_draw_arc) for the indicator.
 * https://developer.repebble.com/guides/graphics-and-animations/animations/
 */
#define LOADING_TIMEOUT_MS 15000
#define LOADING_SPIN_MS 1000
#define LOADING_ARC_PX 32
#define LOADING_ARC_DEG 68
/* Tight stroke: readable on 1-bit, not heavy on color (skill: small screens, dynamic bounds). */
#define LOADING_STROKE_PX PBL_IF_COLOR_ELSE(2, 1)
#define LOADING_LABEL_GAP 0
#define LOADING_TEXT_H 22

static Layer *s_backdrop;
static AppTimer *s_timeout_timer;
static int32_t s_rotation;

static void draw_arc_clipped(GContext *ctx, GRect rect, int32_t start, int32_t span) {
  int32_t a0 = start % TRIG_MAX_ANGLE;
  if (a0 < 0) {
    a0 += TRIG_MAX_ANGLE;
  }
  int32_t a1 = a0 + span;
  if (a1 <= TRIG_MAX_ANGLE) {
    graphics_draw_arc(ctx, rect, GOvalScaleModeFitCircle, a0, a1);
  } else {
    graphics_draw_arc(ctx, rect, GOvalScaleModeFitCircle, a0, TRIG_MAX_ANGLE);
    graphics_draw_arc(ctx, rect, GOvalScaleModeFitCircle, 0, a1 - TRIG_MAX_ANGLE);
  }
}

static void loading_draw(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, theme_bg());
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  int w = LOADING_ARC_PX;
  const int h_pad = 2;
  int block_h = w + LOADING_LABEL_GAP + LOADING_TEXT_H;
  int top = (b.size.h - block_h) / 2;
  GRect arc_rect = GRect((b.size.w - w) / 2, top, w, w);
  int32_t span = DEG_TO_TRIGANGLE(LOADING_ARC_DEG);

  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_stroke_color(ctx, theme_loading_indicator());
  graphics_context_set_stroke_width(ctx, LOADING_STROKE_PX);
  draw_arc_clipped(ctx, arc_rect, s_rotation, span);

  graphics_context_set_antialiased(ctx, true);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GRect text_box = GRect(h_pad, top + w + LOADING_LABEL_GAP, b.size.w - 2 * h_pad, LOADING_TEXT_H);
  graphics_context_set_text_color(ctx, theme_text());
  graphics_draw_text(ctx, "Loading", font, text_box, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

static AppTimer *s_spin_timer = NULL;

static void spin_timer_cb(void *data) {
  s_rotation += (TRIG_MAX_ANGLE / 30); // rotate roughly 30 times a sec
  if (s_backdrop) {
    layer_mark_dirty(s_backdrop);
    s_spin_timer = app_timer_register(33, spin_timer_cb, NULL);
  }
}

static void timeout_cb(void *data) {
  (void)data;
  s_timeout_timer = NULL;
  ui_loading_stop();
}

void ui_loading_start(Layer *window_root_layer) {
  if (!window_root_layer) {
    return;
  }
  ui_loading_stop();

  s_backdrop = layer_create(layer_get_bounds(window_root_layer));
  layer_set_update_proc(s_backdrop, loading_draw);
  layer_add_child(window_root_layer, s_backdrop);

  s_rotation = 0;
  s_spin_timer = app_timer_register(33, spin_timer_cb, NULL);
  s_timeout_timer = app_timer_register(LOADING_TIMEOUT_MS, timeout_cb, NULL);
}

void ui_loading_stop(void) {
  if (s_timeout_timer) {
    app_timer_cancel(s_timeout_timer);
    s_timeout_timer = NULL;
  }
  if (s_spin_timer) {
    app_timer_cancel(s_spin_timer);
    s_spin_timer = NULL;
  }
  if (s_backdrop) {
    layer_remove_from_parent(s_backdrop);
    layer_destroy(s_backdrop);
    s_backdrop = NULL;
  }
}

void ui_loading_invalidate(void) {
  if (s_backdrop) {
    layer_mark_dirty(s_backdrop);
  }
}
