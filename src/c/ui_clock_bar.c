#include "ui_clock_bar.h"
#include "theme.h"

#include <pebble.h>
#include <stdio.h>
#include <time.h>

#define MAX_BARS 4
static Layer *s_bars[MAX_BARS];
static int s_bar_n;

/** strftime "%b %e" space-pads the day, which reads as an oversized gap after the month. */
static const char k_month_abbr[12][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static void time_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, theme_bg());
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  static char tbuf[16];
  tbuf[0] = '\0';
  clock_copy_time_string(tbuf, sizeof(tbuf));
  if (!tbuf[0]) {
    return;
  }

  static char dbuf[20];
  dbuf[0] = '\0';
  time_t now = time(NULL);
  struct tm *lt = localtime(&now);
  if (lt && lt->tm_mon >= 0 && lt->tm_mon < 12) {
    snprintf(dbuf, sizeof(dbuf), "%s %d", k_month_abbr[lt->tm_mon], lt->tm_mday);
  }

  graphics_context_set_text_color(ctx, theme_status_fg());
  graphics_context_set_antialiased(ctx, true);

  GFont bar_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  int date_w = 0;
  if (dbuf[0] != '\0') {
    GSize dsz = graphics_text_layout_get_content_size_with_attributes(
        dbuf, bar_font, GRect(0, 0, b.size.w, b.size.h), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentRight, NULL);
    date_w = (int)dsz.w;
    if (date_w < 0) {
      date_w = 0;
    }
    if (date_w > b.size.w / 2) {
      date_w = b.size.w / 2;
    }
  }
  int gap = 4;
  int time_w = b.size.w - 8 - date_w - gap;
  if (time_w < 20) {
    time_w = b.size.w - 8;
    date_w = 0;
  }

  GRect tr = GRect(4, 0, time_w, b.size.h);
  graphics_draw_text(ctx, tbuf, bar_font, tr, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  if (date_w > 0) {
    GRect dr = GRect(b.size.w - 4 - date_w, 0, date_w, b.size.h);
    graphics_draw_text(ctx, dbuf, bar_font, dr, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  }
}

static void tick(struct tm *tick_time, TimeUnits u) {
  (void)tick_time;
  (void)u;
  for (int i = 0; i < s_bar_n; i++) {
    if (s_bars[i]) {
      layer_mark_dirty(s_bars[i]);
    }
  }
}

static void register_bar(Layer *l) {
  for (int i = 0; i < s_bar_n; i++) {
    if (s_bars[i] == l) {
      return;
    }
  }
  if (s_bar_n < MAX_BARS) {
    s_bars[s_bar_n++] = l;
  }
  if (s_bar_n == 1) {
    tick_timer_service_subscribe(MINUTE_UNIT | DAY_UNIT, tick);
  }
}

static void remove_bar(Layer *l) {
  int w = 0;
  for (int i = 0; i < s_bar_n; i++) {
    if (s_bars[i] != l) {
      s_bars[w++] = s_bars[i];
    }
  }
  s_bar_n = w;
  for (int j = s_bar_n; j < MAX_BARS; j++) {
    s_bars[j] = NULL;
  }
  if (s_bar_n == 0) {
    tick_timer_service_unsubscribe();
  }
}

Layer *ui_clock_bar_create_for_window_size(GSize size) {
  Layer *l = layer_create(GRect(0, 0, size.w, UI_CLOCK_BAR_HEIGHT));
  layer_set_update_proc(l, time_proc);
  register_bar(l);
  layer_mark_dirty(l);
  return l;
}

void ui_clock_bar_unlink_and_destroy(Layer *clock_bar) {
  if (!clock_bar) {
    return;
  }
  remove_bar(clock_bar);
  layer_remove_from_parent(clock_bar);
  layer_destroy(clock_bar);
}

void ui_clock_bar_invalidate(Layer *clock_bar) {
  if (clock_bar) {
    layer_mark_dirty(clock_bar);
  }
}
