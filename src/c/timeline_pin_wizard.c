#include "timeline_pin_wizard.h"

#include "messaging.h"
#include "protocol.h"
#include "theme.h"
#include "ui_clock_bar.h"
#include "ui_toast.h"

#include <pebble.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef enum {
  TP_FOCUS_MONTH = 0,
  TP_FOCUS_DAY = 1,
  TP_FOCUS_YEAR = 2,
  TP_FOCUS_HOUR = 3,
  TP_FOCUS_MIN = 4,
} TpFocus;

static Window *s_win;
static Layer *s_canvas;
#ifndef PBL_ROUND
static Layer *s_clock_bar;
#endif

static int s_list_ix;
static int s_task_ix;

static TpFocus s_focus;
static int s_month;
static int s_day;
static int s_year;
/** 0–23 */
static int s_hour;
/** 0, 5, …, 55 */
static int s_minute;

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

static void tp_adjust_month(int delta) {
  s_month += delta;
  while (s_month > 12) {
    s_month -= 12;
  }
  while (s_month < 1) {
    s_month += 12;
  }
  clamp_day();
}

static void tp_adjust_day(int delta) {
  int dim = days_in_month(s_month, s_year);
  s_day += delta;
  if (s_day > dim) {
    s_day = 1;
  }
  if (s_day < 1) {
    s_day = dim;
  }
}

static void tp_adjust_year(int delta) {
  s_year += delta;
  if (s_year > YEAR_MAX) {
    s_year = YEAR_MIN;
  }
  if (s_year < YEAR_MIN) {
    s_year = YEAR_MAX;
  }
  clamp_day();
}

static void tp_adjust_hour(int delta) {
  s_hour += delta;
  if (s_hour > 23) {
    s_hour = 0;
  }
  if (s_hour < 0) {
    s_hour = 23;
  }
}

static void tp_adjust_minute(int delta) {
  s_minute += delta * 5;
  if (s_minute > 55) {
    s_minute = 0;
  }
  if (s_minute < 0) {
    s_minute = 55;
  }
}

/** Parse leading YYYY-MM-DD without sscanf (avoids heavy libc hooks on some FW). */
static bool tp_parse_iso_ymd(const char *s, int *out_y, int *out_m, int *out_d) {
  if (!s || strlen(s) < 10) {
    return false;
  }
  for (int i = 0; i < 4; i++) {
    if (s[i] < '0' || s[i] > '9') {
      return false;
    }
  }
  if (s[4] != '-') {
    return false;
  }
  for (int i = 5; i < 7; i++) {
    if (s[i] < '0' || s[i] > '9') {
      return false;
    }
  }
  if (s[7] != '-') {
    return false;
  }
  for (int i = 8; i < 10; i++) {
    if (s[i] < '0' || s[i] > '9') {
      return false;
    }
  }
  *out_y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
  *out_m = (s[5] - '0') * 10 + (s[6] - '0');
  *out_d = (s[8] - '0') * 10 + (s[9] - '0');
  return true;
}

static void tp_send_and_close(void) {
  clamp_day();
  clamp_year();

  struct tm local_tm = {0};
  local_tm.tm_year = s_year - 1900;
  local_tm.tm_mon = s_month - 1;
  local_tm.tm_mday = s_day;
  local_tm.tm_hour = s_hour;
  local_tm.tm_min = s_minute;
  local_tm.tm_sec = 0;
  local_tm.tm_isdst = -1;

  time_t tloc = mktime(&local_tm);
  if (tloc == (time_t)-1) {
    return;
  }

  struct tm *utc = gmtime(&tloc);
  static char iso[40];
  snprintf(iso, sizeof(iso), "%04d-%02d-%02dT%02d:%02d:%02dZ", utc->tm_year + 1900, utc->tm_mon + 1,
           utc->tm_mday, utc->tm_hour, utc->tm_min, utc->tm_sec);

  messaging_send(CMD_W_PIN_TASK, s_list_ix, s_task_ix, iso);
  ui_toast_show("Adding to timeline…");
  window_stack_pop(true);
}

static void tp_apply_due_iso_defaults(const char *due_iso) {
  s_hour = 9;
  s_minute = 0;

  int y = 0, m = 0, d = 0;
  if (due_iso && due_iso[0] && tp_parse_iso_ymd(due_iso, &y, &m, &d)) {
    if (y >= YEAR_MIN && y <= YEAR_MAX && m >= 1 && m <= 12) {
      s_year = y;
      s_month = m;
      s_day = d;
      clamp_year();
      clamp_day();
      return;
    }
  }

  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  s_month = tm->tm_mon + 1;
  s_day = tm->tm_mday;
  s_year = tm->tm_year + 1900;
  clamp_year();
  clamp_day();
}

static void tp_canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, theme_bg());
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont digit_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont small_digit = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

  const char *title = "Pin time";
  GRect title_rect = GRect(0, PBL_IF_ROUND_ELSE(6, 2), b.size.w, 20);
  graphics_context_set_text_color(ctx, theme_text());
  graphics_draw_text(ctx, title, title_font, title_rect, GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  int gap = 4;
  int date_top = PBL_IF_ROUND_ELSE(30, 22);
  int row_h = PBL_IF_ROUND_ELSE(44, 40);
  int inner_w = b.size.w - 16;
  int x0 = 8;
  int box_w3 = (inner_w - 2 * gap) / 3;

#ifdef PBL_COLOR
  GColor inactive_bg = theme_toast_bg();
  GColor inactive_digit = GColorLightGray;
#else
  GColor inactive_bg = GColorDarkGray;
  GColor inactive_digit = theme_text();
#endif

  char buf[8];

  /* Date row: month, day, year */
  for (int i = 0; i < 3; i++) {
    GRect box = GRect(x0 + i * (box_w3 + gap), date_top, box_w3, row_h);
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
    GRect tin = GRect(box.origin.x, box.origin.y + PBL_IF_ROUND_ELSE(8, 6), box.size.w, box.size.h);
    graphics_draw_text(ctx, buf, digit_font, tin, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }

  GFont hint_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_context_set_text_color(ctx, theme_text());
  const char *dlab[] = {"Mo", "Day", "YY"};
  for (int j = 0; j < 3; j++) {
    GRect lr =
        GRect(x0 + j * (box_w3 + gap), date_top + row_h + 1, box_w3, 14);
    graphics_draw_text(ctx, dlab[j], hint_font, lr, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }

  /* Time row */
  int time_top = date_top + row_h + 18;
  int box_w2 = (inner_w - gap) / 2;

  for (int k = 0; k < 2; k++) {
    GRect box = GRect(x0 + k * (box_w2 + gap), time_top, box_w2, row_h);
    bool sel = ((int)s_focus == (3 + k));
    graphics_context_set_fill_color(ctx, sel ? theme_highlight_bg() : inactive_bg);
    graphics_fill_rect(ctx, box, 4, GCornersAll);
    graphics_context_set_text_color(ctx, sel ? theme_highlight_text() : inactive_digit);
    if (k == 0) {
      snprintf(buf, sizeof(buf), "%02d", s_hour);
    } else {
      snprintf(buf, sizeof(buf), "%02d", s_minute);
    }
    GRect tin2 = GRect(box.origin.x, box.origin.y + PBL_IF_ROUND_ELSE(8, 6), box.size.w, box.size.h);
    graphics_draw_text(ctx, buf, small_digit, tin2, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }

  const char *tlab[] = {"Hour", "Min"};
  for (int t = 0; t < 2; t++) {
    GRect lr = GRect(x0 + t * (box_w2 + gap), time_top + row_h + 1, box_w2, 14);
    graphics_draw_text(ctx, tlab[t], hint_font, lr, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }

  GFont foot_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  graphics_context_set_text_color(ctx, theme_text());
  graphics_draw_text(ctx, "5 min per step", foot_font,
                     GRect(0, b.size.h - PBL_IF_ROUND_ELSE(18, 14), b.size.w, 14), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
}

static void tp_mark_dirty(void) {
  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }
}

static void tp_up_handler(ClickRecognizerRef recognizer, void *ctx) {
  (void)recognizer;
  (void)ctx;
  switch (s_focus) {
    case TP_FOCUS_MONTH:
      tp_adjust_month(1);
      break;
    case TP_FOCUS_DAY:
      tp_adjust_day(1);
      break;
    case TP_FOCUS_YEAR:
      tp_adjust_year(1);
      break;
    case TP_FOCUS_HOUR:
      tp_adjust_hour(1);
      break;
    default:
      tp_adjust_minute(1);
      break;
  }
  tp_mark_dirty();
}

static void tp_down_handler(ClickRecognizerRef recognizer, void *ctx) {
  (void)recognizer;
  (void)ctx;
  switch (s_focus) {
    case TP_FOCUS_MONTH:
      tp_adjust_month(-1);
      break;
    case TP_FOCUS_DAY:
      tp_adjust_day(-1);
      break;
    case TP_FOCUS_YEAR:
      tp_adjust_year(-1);
      break;
    case TP_FOCUS_HOUR:
      tp_adjust_hour(-1);
      break;
    default:
      tp_adjust_minute(-1);
      break;
  }
  tp_mark_dirty();
}

static void tp_select_handler(ClickRecognizerRef recognizer, void *ctx) {
  (void)recognizer;
  (void)ctx;
  if ((int)s_focus < (int)TP_FOCUS_MIN) {
    s_focus = (TpFocus)((int)s_focus + 1);
    tp_mark_dirty();
    return;
  }
  tp_send_and_close();
}

static void tp_back_handler(ClickRecognizerRef recognizer, void *ctx) {
  (void)recognizer;
  (void)ctx;
  if (s_focus > TP_FOCUS_MONTH) {
    s_focus = (TpFocus)((int)s_focus - 1);
    tp_mark_dirty();
    return;
  }
  window_stack_pop(true);
}

static void tp_click_config(void *ctx) {
  (void)ctx;
  window_single_click_subscribe(BUTTON_ID_UP, tp_up_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, tp_down_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, tp_select_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, tp_back_handler);
}

static void tp_load(Window *w) {
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
  layer_set_update_proc(s_canvas, tp_canvas_update);
  layer_add_child(wl, s_canvas);

  window_set_click_config_provider_with_context(s_win, tp_click_config, NULL);

  s_focus = TP_FOCUS_MONTH;
  tp_mark_dirty();
}

static void tp_unload(Window *w) {
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

void timeline_pin_wizard_push(int list_index, int task_index, const char *due_iso_yyyy_mm_dd_or_null) {
  if (s_win) {
    return;
  }
  s_list_ix = list_index;
  s_task_ix = task_index;
  tp_apply_due_iso_defaults(due_iso_yyyy_mm_dd_or_null);

  s_focus = TP_FOCUS_MONTH;

  s_win = window_create();
  window_set_window_handlers(s_win, (WindowHandlers){.load = tp_load, .unload = tp_unload});
  window_set_background_color(s_win, theme_bg());
  window_stack_push(s_win, true);
}

void timeline_pin_wizard_apply_theme(void) {
  if (!s_win) {
    return;
  }
  window_set_background_color(s_win, theme_bg());
#ifndef PBL_ROUND
  ui_clock_bar_invalidate(s_clock_bar);
#endif
  tp_mark_dirty();
}
