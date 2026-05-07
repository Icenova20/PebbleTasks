#include "theme.h"

#include "completed_menu.h"
#include "due_wizard.h"
#include "main_menu.h"
#include "list_action_menu.h"
#include "task_action_menu.h"
#include "task_full_view.h"
#include "tasks_menu.h"
#include "timeline_pin_wizard.h"
#include "ui_loading.h"
#include "ui_toast.h"

#include <pebble.h>

/*
 * Google Tasks–adjacent blues (Material / Google brand #4285F4, secondary ~#8AB4F8).
 * GColorFromRGB maps to dithered gray on BW platforms.
 */
#define TASKS_BLUE_PRIMARY GColorFromRGB(66, 133, 244)
#define TASKS_BLUE_ACCENT GColorFromRGB(138, 180, 248)
/** Loading arc: darker than primary so it reads clearly on white (#1565C0). */
#define TASKS_BLUE_LOADING GColorFromRGB(21, 101, 192)

static GColor s_bg, s_text, s_hi_bg, s_hi_text, s_accent, s_status_fg, s_toast_bg, s_toast_text;

static void load_palette(void) {
  s_bg = GColorWhite;
  s_text = GColorBlack;
  s_hi_bg = TASKS_BLUE_PRIMARY;
  /* White on primary blue (Tasks / Material filled buttons). */
  s_hi_text = GColorWhite;
  s_accent = TASKS_BLUE_ACCENT;
  s_status_fg = GColorBlack;
#ifdef PBL_COLOR
  s_toast_bg = GColorDarkGray;
  s_toast_text = GColorWhite;
#else
  /* BW: high-contrast toast bar (dark gray dithers poorly on mono). */
  s_toast_bg = GColorWhite;
  s_toast_text = GColorBlack;
#endif
}

void theme_init(void) {
  load_palette();
}

GColor theme_bg(void) { return s_bg; }
GColor theme_text(void) { return s_text; }
GColor theme_highlight_bg(void) { return s_hi_bg; }
GColor theme_highlight_text(void) { return s_hi_text; }

GColor theme_menu_highlight_bg(void) {
  return s_hi_bg;
}

GColor theme_menu_highlight_text(void) {
  return s_text;
}

GColor theme_accent(void) { return s_accent; }

GColor theme_loading_indicator(void) {
#ifdef PBL_COLOR
  return TASKS_BLUE_LOADING;
#else
  return GColorBlack;
#endif
}

GColor theme_status_fg(void) { return s_status_fg; }
GColor theme_toast_bg(void) { return s_toast_bg; }
GColor theme_toast_text(void) { return s_toast_text; }

void theme_apply_all(void) {
  main_menu_apply_theme();
  tasks_menu_apply_theme();
  completed_menu_apply_theme();
  task_action_menu_apply_theme();
  list_action_menu_apply_theme();
  task_full_view_apply_theme();
  due_wizard_apply_theme();
  timeline_pin_wizard_apply_theme();
  ui_toast_apply_theme();
  ui_loading_invalidate();
}
