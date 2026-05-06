#include "due_wizard.h"

#include "messaging.h"
#include "protocol.h"
#include "theme.h"
#include "ui_clock_bar.h"

#include <pebble.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef enum { FOCUS_MONTH = 0, FOCUS_DAY = 1, FOCUS_YEAR = 2 } DueFocus;

static Window *s_win;
static Layer *s_canvas;
#ifndef PBL_ROUND
static Layer *s_clock_bar;
#endif

static DueFocus s_focus;
static int s_list_ix;
static int s_task_ix;
/** 1–12, 1–31, full year (UI shows last two digits only) */
static int s_month;
static int s_day;
static int s_year;

#define YEAR_MIN 2020
#define YEAR_MAX 2040

static bool is_leap(int y) {
  return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
}

static int days_in_month(int month, int year) {
  static const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 31;
  }
  int d = dim[month - 1];
  if (month == 2 && is_leap(year)) {
    d = 29;
  }
  return d;
}

static void clamp_day(void) {
  int mx = days_in_month(s_month, s_year);
  if (s_day > mx) {
    s_day = mx;
  }
  if (s_day < 1) {
    s_day = 1;
  }
}

static void clamp_year(void) {
  if (s_year < YEAR_MIN) {
    s_year = YEAR_MIN;
  }
  if (s_year > YEAR_MAX) {
    s_year = YEAR_MAX;
  }
}

static void due_send_and_close(void) {
  clamp_day();
  clamp_year();
  static char iso[16];
  snprintf(iso, sizeof(iso), "%04d-%02d-%02d", s_year, s_month, s_day);
  messaging_send(CMD_W_SET_TASK_DUE, s_list_ix, s_task_ix, iso);
  window_stack_pop(true);
}

static void due_adjust_month(int delta) {
  s_month += delta;
  while (s_month > 12) {
    s_month -= 12;
  }
  while (s_month < 1) {
    s_month += 12;
  }
  clamp_day();
}

static void due_adjust_day(int delta) {
  int dim = days_in_month(s_month, s_year);
  s_day += delta;
  if (s_day > dim) {
    s_day = 1;
  }
  if (s_day < 1) {
    s_day = dim;
  }
}

static void due_adjust_year(int delta) {
  s_year += delta;
  if (s_year > YEAR_MAX) {
    s_year = YEAR_MIN;
  }
  if (s_year < YEAR_MIN) {
    s_year = YEAR_MAX;
  }
  clamp_day();
}

static void due_canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, theme_bg());
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont digit_font = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

  const char *title = "Due date";
  GRect title_rect = GRect(0, PBL_IF_ROUND_ELSE(8, 4), b.size.w, 22);
  graphics_context_set_text_color(ctx, theme_text());
  graphics_draw_text(ctx, title, title_font, title_rect, GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  int gap = 4;
  int row_top = PBL_IF_ROUND_ELSE(44, 36);
  int row_h = PBL_IF_ROUND_ELSE(52, 48);
  int inner_w = b.size.w - 16;
  int x0 = 8;
  /* Equal thirds */
  int box_w = (inner_w - 2 * gap) / 3;

#ifdef PBL_COLOR
  GColor inactive_bg = theme_toast_bg();
  GColor inactive_digit = GColorLightGray;
#else
  /* BW: inactive pills must not share the same fill as selected (both were white). */
  GColor inactive_bg = GColorDarkGray;
  GColor inactive_digit = theme_text();
#endif

  char buf[8];
  for (int i = 0; i < 3; i++) {
    GRect box = GRect(x0 + i * (box_w + gap), row_top, box_w, row_h);
    bool sel = ((int)s_focus == i);
    graphics_context_set_fill_color(ctx, sel ? theme_highlight_bg() : inactive_bg);
    graphics_fill_rect(ctx, box, 4, GCornersAll);
    graphics_context_set_text_color(ctx, sel ? theme_highlight_text() : inactive_digit);
    switch (i) {
      case 0:
        snprintf(buf, sizeof(buf), "%02d", s_month);
        break;
      case 1:
        snprintf(buf, sizeof(buf), "%02d", s_day);
        break;
      default:
        snprintf(buf, sizeof(buf), "%02d", s_year % 100);
        break;
    }
    GRect text_in = GRect(box.origin.x, box.origin.y + PBL_IF_ROUND_ELSE(8, 6), box.size.w, box.size.h);
    graphics_draw_text(ctx, buf, digit_font, text_in, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }

  GFont hint_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  graphics_context_set_text_color(ctx, theme_text());
  const char *labels[] = {"Month", "Day", "YY"};
  for (int j = 0; j < 3; j++) {
    GRect lr = GRect(x0 + j * (box_w + gap), row_top + row_h + 2, box_w, 16);
    graphics_draw_text(ctx, labels[j], hint_font, lr, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

static void due_mark_dirty(void) {
  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }
}

static void due_up_handler(ClickRecognizerRef recognizer, void *ctx) {
  (void)recognizer;
  (void)ctx;
  switch (s_focus) {
    case FOCUS_MONTH:
      due_adjust_month(1);
      break;
    case FOCUS_DAY:
      due_adjust_day(1);
      break;
    default:
      due_adjust_year(1);
      break;
  }
  due_mark_dirty();
}

static void due_down_handler(ClickRecognizerRef recognizer, void *ctx) {
  (void)recognizer;
  (void)ctx;
  switch (s_focus) {
    case FOCUS_MONTH:
      due_adjust_month(-1);
      break;
    case FOCUS_DAY:
      due_adjust_day(-1);
      break;
    default:
      due_adjust_year(-1);
      break;
  }
  due_mark_dirty();
}

static void due_select_handler(ClickRecognizerRef recognizer, void *ctx) {
  (void)recognizer;
  (void)ctx;
  if (s_focus < FOCUS_YEAR) {
    s_focus = (DueFocus)((int)s_focus + 1);
    clamp_day();
    due_mark_dirty();
    return;
  }
  due_send_and_close();
}

static void due_back_handler(ClickRecognizerRef recognizer, void *ctx) {
  (void)recognizer;
  (void)ctx;
  if (s_focus > FOCUS_MONTH) {
    s_focus = (DueFocus)((int)s_focus - 1);
    due_mark_dirty();
    return;
  }
  window_stack_pop(true);
}

static void due_click_config(void *ctx) {
  (void)ctx;
  window_single_click_subscribe(BUTTON_ID_UP, due_up_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, due_down_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, due_select_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, due_back_handler);
}

static void due_load(Window *w) {
  (void)w;
  Layer *wl = window_get_root_layer(s_win);
  GRect wb = layer_get_bounds(wl);

#ifdef PBL_ROUND
  GRect cb = wb;
#else
  GRect cb = GRect(0, UI_CLOCK_BAR_HEIGHT, wb.size.w, wb.size.h - UI_CLOCK_BAR_HEIGHT);
#endif

#ifndef PBL_ROUND
  s_clock_bar = ui_clock_bar_create_for_window_size(wb.size);
  if (s_clock_bar) {
    layer_add_child(wl, s_clock_bar);
  }
#endif

  s_canvas = layer_create(cb);
  layer_set_update_proc(s_canvas, due_canvas_update);
  layer_add_child(wl, s_canvas);

  window_set_click_config_provider_with_context(s_win, due_click_config, NULL);

  s_focus = FOCUS_MONTH;
  due_mark_dirty();
}

static void due_unload(Window *w) {
  (void)w;
#ifndef PBL_ROUND
  ui_clock_bar_unlink_and_destroy(s_clock_bar);
  s_clock_bar = NULL;
#endif
  if (s_canvas) {
    layer_destroy(s_canvas);
    s_canvas = NULL;
  }
  s_win = NULL;
}

void due_wizard_push(int list_index, int task_index) {
  s_list_ix = list_index;
  s_task_ix = task_index;

  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  s_month = tm->tm_mon + 1;
  s_day = tm->tm_mday;
  s_year = tm->tm_year + 1900;
  clamp_year();
  clamp_day();

  s_focus = FOCUS_MONTH;

  if (s_win) {
    return;
  }
  s_win = window_create();
  window_set_window_handlers(s_win, (WindowHandlers){.load = due_load, .unload = due_unload});
  window_set_background_color(s_win, theme_bg());
  window_stack_push(s_win, true);
}

void due_wizard_apply_theme(void) {
  if (!s_win) {
    return;
  }
  window_set_background_color(s_win, theme_bg());
#ifndef PBL_ROUND
  ui_clock_bar_invalidate(s_clock_bar);
#endif
  due_mark_dirty();
}
