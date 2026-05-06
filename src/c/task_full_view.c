#include "task_full_view.h"

#include "theme.h"
#include <pebble.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FULL_VIEW_MARGIN 10

static char *duplicate_cstr(const char *s) {
  size_t n = strlen(s) + 1;
  char *out = malloc(n);
  if (out) {
    memcpy(out, s, n);
  }
  return out;
}

#ifdef PBL_ROUND
static GTextAttributes *s_full_attrs;

static GTextAttributes *full_attrs(void) {
  if (!s_full_attrs) {
    s_full_attrs = graphics_text_attributes_create();
    graphics_text_attributes_enable_screen_text_flow(s_full_attrs, FULL_VIEW_MARGIN * 2);
  }
  return s_full_attrs;
}
#else
#define full_attrs() ((GTextAttributes *)NULL)
#endif

static Window *s_win;
static ScrollLayer *s_scroll;
static TextLayer *s_text;
static char *s_body;

static void full_view_window_unload(Window *w) {
  (void)w;
  if (s_text) {
    text_layer_destroy(s_text);
    s_text = NULL;
  }
  if (s_scroll) {
    scroll_layer_destroy(s_scroll);
    s_scroll = NULL;
  }
  free(s_body);
  s_body = NULL;
  s_win = NULL;
}

static void full_view_window_load(Window *w) {
  if (!s_body || !w) {
    return;
  }

  Layer *root = window_get_root_layer(w);
  GRect wb = layer_get_bounds(root);

  window_set_background_color(w, theme_bg());

  s_scroll = scroll_layer_create(wb);
  scroll_layer_set_click_config_onto_window(s_scroll, w);
  scroll_layer_set_callbacks(s_scroll, (ScrollLayerCallbacks){0});

  int tw = wb.size.w - 2 * FULL_VIEW_MARGIN;
  if (tw < 8) {
    tw = 8;
  }
  GTextAlignment tal = PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft);
  GFont f = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GSize cs = graphics_text_layout_get_content_size_with_attributes(s_body, f, GRect(0, 0, tw, 5000),
                                                                   GTextOverflowModeWordWrap, tal,
                                                                   full_attrs());
  GSize line_probe = graphics_text_layout_get_content_size_with_attributes(
      "Mg", f, GRect(0, 0, tw, 100), GTextOverflowModeTrailingEllipsis, tal, full_attrs());
  int th = (cs.h <= 1) ? (int)line_probe.h : cs.h;

  int content_h = th + 2 * FULL_VIEW_MARGIN;
  if (content_h < wb.size.h) {
    content_h = wb.size.h;
  }

  s_text = text_layer_create(GRect(FULL_VIEW_MARGIN, FULL_VIEW_MARGIN, tw, th));
  text_layer_set_text(s_text, s_body);
  text_layer_set_font(s_text, f);
  text_layer_set_text_color(s_text, theme_text());
  text_layer_set_background_color(s_text, GColorClear);
  text_layer_set_overflow_mode(s_text, GTextOverflowModeWordWrap);
  text_layer_set_text_alignment(s_text, tal);

  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_text));
  scroll_layer_set_content_size(s_scroll, GSize(wb.size.w, content_h));

  layer_add_child(root, scroll_layer_get_layer(s_scroll));
}

void task_full_view_push(const char *title, const char *due_or_null) {
  if (s_win) {
    return;
  }

  free(s_body);
  s_body = NULL;

  const char *t = title ? title : "";
  size_t tlen = strlen(t);
  if (due_or_null && due_or_null[0]) {
    size_t dlen = strlen(due_or_null);
    size_t need = tlen + dlen + 32;
    s_body = malloc(need);
    if (!s_body) {
      return;
    }
    snprintf(s_body, need, "%s\n\nDue: %s", t, due_or_null);
  } else {
    s_body = duplicate_cstr(t);
    if (!s_body) {
      return;
    }
  }

  s_win = window_create();
  window_set_window_handlers(s_win, (WindowHandlers){.load = full_view_window_load, .unload = full_view_window_unload});
  window_set_background_color(s_win, theme_bg());
  window_stack_push(s_win, true);
}

void task_full_view_apply_theme(void) {
  if (!s_win) {
    return;
  }
  window_set_background_color(s_win, theme_bg());
  if (s_text) {
    text_layer_set_text_color(s_text, theme_text());
    text_layer_set_background_color(s_text, GColorClear);
  }
}
