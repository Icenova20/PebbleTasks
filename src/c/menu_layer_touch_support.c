#include "menu_layer_touch_support.h"

#ifdef PBL_TOUCH

#include <stdint.h>
#include <string.h>

/** Finger movement past this (px) before drag-scroll replaces tap selection for this gesture. */
#define DRAG_SCROLL_THRESHOLD 12

static MenuLayerTouchHooks s_hooks;
static bool s_hooks_ready;
static int s_pressed_row;

static int16_t s_finger_start_y;
static int32_t s_scroll_y_at_touchdown;
static bool s_drag_scrolling;

static uint16_t prv_num_rows(void) {
  if (!s_hooks_ready || !s_hooks.menu || !s_hooks.get_num_rows) {
    return 0;
  }
  return s_hooks.get_num_rows(s_hooks.menu, 0, s_hooks.callback_context);
}

static bool prv_touch_to_content_y(const TouchEvent *event, MenuLayer *menu, int16_t *out_content_y) {
  if (!menu || !event) {
    return false;
  }

  ScrollLayer *scroll = menu_layer_get_scroll_layer(menu);
  if (!scroll) {
    return false;
  }
  Layer *scroll_layer_el = scroll_layer_get_layer(scroll);
  GRect scroll_frame = layer_get_bounds(scroll_layer_el);
  scroll_frame = layer_convert_rect_to_screen(scroll_layer_el, scroll_frame);

  GPoint p = GPoint(event->x, event->y);
  if (!grect_contains_point(&scroll_frame, &p)) {
    return false;
  }

  int32_t y_in_viewport = (int32_t)p.y - (int32_t)scroll_frame.origin.y;

  GPoint offset = scroll_layer_get_content_offset(scroll);
  int32_t content_y = y_in_viewport - (int32_t)offset.y;
  if (content_y < 0) {
    return false;
  }
  *out_content_y = (int16_t)((content_y > INT16_MAX) ? INT16_MAX : content_y);
  return true;
}

static int prv_row_at_content_y(int16_t content_y) {
  if (!s_hooks_ready || !s_hooks.menu || !s_hooks.get_num_rows || !s_hooks.get_cell_height) {
    return -1;
  }
  uint16_t rows = prv_num_rows();
  int32_t y = content_y;

  MenuIndex idx = {.section = 0, .row = 0};
  for (; idx.row < rows; idx.row++) {
    int16_t cell_h =
        s_hooks.get_cell_height(s_hooks.menu, &idx, s_hooks.callback_context);
    if (cell_h <= 0) {
      cell_h = 40;
    }
    if ((int32_t)cell_h > y) {
      return idx.row;
    }
    y -= (int32_t)cell_h;
  }
  return -1;
}

static int prv_row_from_event(const TouchEvent *event, MenuLayer *menu) {
  int16_t content_y = 0;
  if (!prv_touch_to_content_y(event, menu, &content_y)) {
    return -1;
  }
  return prv_row_at_content_y(content_y);
}

static bool prv_contains_point(MenuLayer *menu, const TouchEvent *event) {
  if (!menu || !event) {
    return false;
  }
  Layer *l = menu_layer_get_layer(menu);
  GRect m = layer_get_bounds(l);
  m = layer_convert_rect_to_screen(l, m);
  GPoint p = GPoint(event->x, event->y);
  return grect_contains_point(&m, &p);
}

static int32_t prv_i32_abs(int32_t x) { return x < 0 ? -x : x; }

static void prv_clamp_and_set_scroll_y(ScrollLayer *scroll, int32_t desired_y) {
  if (!scroll) {
    return;
  }
  Layer *layer = scroll_layer_get_layer(scroll);
  GRect b = layer_get_bounds(layer);
  GSize cs = scroll_layer_get_content_size(scroll);
  int32_t viewport_h = b.size.h;
  int32_t content_h = cs.h;
  int32_t max_y = 0;
  int32_t min_y = (content_h > viewport_h) ? -(content_h - viewport_h) : 0;
  if (desired_y > max_y) {
    desired_y = max_y;
  }
  if (desired_y < min_y) {
    desired_y = min_y;
  }
  scroll_layer_set_content_offset(scroll, GPoint(0, (int16_t)desired_y), false);
}

static void prv_touch_shutdown(void) {
  s_pressed_row = -1;
  s_drag_scrolling = false;
  s_hooks_ready = false;
  memset(&s_hooks, 0, sizeof(s_hooks));
  touch_service_unsubscribe();
}

static void prv_touch_handler(const TouchEvent *event, void *context) {
  (void)context;

  MenuLayer *menu = s_hooks.menu;

  if (!event || !s_hooks_ready || !menu || !s_hooks.get_num_rows || !s_hooks.get_cell_height ||
      !s_hooks.select_click) {
    return;
  }

  switch (event->type) {
    case TouchEvent_Touchdown: {
      s_drag_scrolling = false;

      if (!prv_contains_point(menu, event)) {
        s_pressed_row = -1;
        break;
      }

      s_finger_start_y = event->y;
      ScrollLayer *scroll = menu_layer_get_scroll_layer(menu);
      if (scroll) {
        s_scroll_y_at_touchdown = scroll_layer_get_content_offset(scroll).y;
      }

      int row = prv_row_from_event(event, menu);
      if (row < 0) {
        s_pressed_row = -1;
        break;
      }
      s_pressed_row = row;

      MenuIndex mi = {.section = 0, .row = (uint16_t)row};
      menu_layer_set_selected_index(menu, mi, MenuRowAlignCenter, false);
      break;
    }
    case TouchEvent_PositionUpdate: {
      ScrollLayer *scroll = menu_layer_get_scroll_layer(menu);
      int32_t fdelta = scroll ? ((int32_t)event->y - (int32_t)s_finger_start_y) : 0;

      if (!s_drag_scrolling && scroll && prv_i32_abs(fdelta) >= DRAG_SCROLL_THRESHOLD) {
        s_drag_scrolling = true;
        s_pressed_row = -1;
      }

      if (s_drag_scrolling && scroll) {
        int32_t new_y = s_scroll_y_at_touchdown + fdelta;
        prv_clamp_and_set_scroll_y(scroll, new_y);
        break;
      }

      int row_now = prv_row_from_event(event, menu);
      if (!prv_contains_point(menu, event) || (row_now >= 0 && row_now != s_pressed_row)) {
        s_pressed_row = -1;
      }
      break;
    }
    case TouchEvent_Liftoff: {
      if (s_drag_scrolling) {
        s_drag_scrolling = false;
        s_pressed_row = -1;
        break;
      }

      if (s_pressed_row < 0) {
        break;
      }
      int row_on_up = prv_row_from_event(event, menu);
      int row_down = s_pressed_row;
      s_pressed_row = -1;

      if (!s_hooks_ready || row_on_up != row_down || row_on_up < 0) {
        break;
      }
      if ((uint16_t)row_down >= prv_num_rows()) {
        break;
      }
      MenuIndex idx = {.section = 0, .row = (uint16_t)row_down};
      s_hooks.select_click(s_hooks.menu, &idx, s_hooks.callback_context);
      break;
    }
    default:
      break;
  }
}

#else

/** No touchscreen — empty definitions. */

#endif

void menu_layer_configure_scroll_behavior(MenuLayer *menu) {
  if (!menu) {
    return;
  }
  ScrollLayer *sl = menu_layer_get_scroll_layer(menu);
  if (!sl) {
    return;
  }
  scroll_layer_set_paging(sl, false);
}

void menu_layer_touch_on_window_appear(const MenuLayerTouchHooks *hooks) {
#ifdef PBL_TOUCH
  if (!touch_service_is_enabled() || !hooks || !hooks->menu) {
    prv_touch_shutdown();
    return;
  }
  prv_touch_shutdown();
  s_hooks = *hooks;
  s_hooks_ready = true;
  touch_service_subscribe(prv_touch_handler, NULL);
#else
  (void)hooks;
#endif
}

void menu_layer_touch_on_window_disappear(void) {
#ifdef PBL_TOUCH
  prv_touch_shutdown();
#endif
}
