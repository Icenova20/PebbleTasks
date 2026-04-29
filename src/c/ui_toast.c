#include "ui_toast.h"
#include "theme.h"
#include "ui_constants.h"

static TextLayer *s_toast;
static AppTimer *s_toast_timer;

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
  text_layer_set_text(s_toast, msg);
  layer_set_hidden(text_layer_get_layer(s_toast), false);
  if (s_toast_timer) {
    app_timer_cancel(s_toast_timer);
  }
  s_toast_timer = app_timer_register(2500, toast_clear_cb, NULL);
}
