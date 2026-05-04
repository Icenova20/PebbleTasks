#include "ui_toast.h"
#include "theme.h"
#include "ui_constants.h"

static TextLayer *s_toast;
static AppTimer *s_toast_timer;
/** Window whose root layer currently owns the toast (SDK has no layer_get_superlayer). */
static Window *s_toast_host_window;

static void toast_clear_cb(void *data) {
  (void)data;
  s_toast_timer = NULL;
  if (s_toast) {
    text_layer_set_text(s_toast, "");
    layer_set_hidden(text_layer_get_layer(s_toast), true);
  }
}

void ui_toast_init(Layer *root_layer) {
  GRect b = layer_get_bounds(root_layer);
  s_toast = text_layer_create(GRect(0, b.size.h - UI_TOAST_HEIGHT, b.size.w, UI_TOAST_HEIGHT));
  text_layer_set_text(s_toast, "");
  text_layer_set_text_alignment(s_toast, GTextAlignmentCenter);
  text_layer_set_font(s_toast, fonts_get_system_font(UI_TOAST_FONT_KEY));
  text_layer_set_background_color(s_toast, theme_toast_bg());
  text_layer_set_text_color(s_toast, theme_toast_text());
  layer_set_hidden(text_layer_get_layer(s_toast), true);
  layer_add_child(root_layer, text_layer_get_layer(s_toast));
  s_toast_host_window = window_stack_get_top_window();
}

void ui_toast_deinit(void) {
  if (s_toast_timer) {
    app_timer_cancel(s_toast_timer);
    s_toast_timer = NULL;
  }
  if (s_toast) {
    text_layer_destroy(s_toast);
    s_toast = NULL;
  }
  s_toast_host_window = NULL;
}

void ui_toast_detach_from_window(Window *window) {
  if (!s_toast || !window || s_toast_host_window != window) {
    return;
  }
  layer_remove_from_parent(text_layer_get_layer(s_toast));
  s_toast_host_window = NULL;
}

/** Toast must live under the top window so it appears above tasks/completed, not behind them. */
static void ui_toast_reparent_to_top(void) {
  if (!s_toast) {
    return;
  }
  Window *top = window_stack_get_top_window();
  if (!top) {
    return;
  }
  if (s_toast_host_window == top) {
    return;
  }
  Layer *tl = text_layer_get_layer(s_toast);
  if (s_toast_host_window) {
    layer_remove_from_parent(tl);
    s_toast_host_window = NULL;
  }
  Layer *root = window_get_root_layer(top);
  GRect b = layer_get_bounds(root);
  layer_set_frame(tl, GRect(0, b.size.h - UI_TOAST_HEIGHT, b.size.w, UI_TOAST_HEIGHT));
  layer_add_child(root, tl);
  s_toast_host_window = top;
}

void ui_toast_apply_theme(void) {
  if (s_toast) {
    text_layer_set_background_color(s_toast, theme_toast_bg());
    text_layer_set_text_color(s_toast, theme_toast_text());
  }
}

void ui_toast_show(const char *msg) {
  if (!s_toast) {
    return;
  }
  ui_toast_reparent_to_top();
  text_layer_set_text(s_toast, msg);
  layer_set_hidden(text_layer_get_layer(s_toast), false);
  if (s_toast_timer) {
    app_timer_cancel(s_toast_timer);
  }
  s_toast_timer = app_timer_register(2500, toast_clear_cb, NULL);
}
