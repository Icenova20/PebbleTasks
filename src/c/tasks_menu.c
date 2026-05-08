#include "tasks_menu.h"
#include "completed_menu.h"
#include "main_menu.h"
#include "messaging.h"
#include "task_action_menu.h"
#include "pebble_tasks.h"
#include "protocol.h"
#include "str_util.h"
#include "theme.h"
#include "ui_assets.h"
#include "ui_clock_bar.h"
#include "ui_constants.h"
#include "ui_menu_wrap_cell.h"
#include "ui_loading.h"
#include "ui_toast.h"
#include "menu_layer_touch_support.h"

#include <string.h>

static Window *s_tasks_window;
static MenuLayer *s_tasks_menu;
#ifndef PBL_ROUND
static Layer *s_tasks_clock;
#endif
static int s_tasks_num_rows;
static char *s_tasks_titles[MAX_MENU_LINES + 1];
static char *s_tasks_due[MAX_MENU_LINES + 1];
/** Machine-readable YYYY-MM-DD when phone sends title\x1Fiso\x1Fdisplay; else NULL. */
static char *s_tasks_due_iso[MAX_MENU_LINES + 1];
static int s_tasks_open_count;
static bool s_tasks_show_completed_nav;
static int s_current_list_index;
static bool s_tasks_window_is_on_stack;

static bool s_tasks_suppress_next_select_click;

/** Pass non-empty due strings as subtitle; omit empty so basic cell is single-line. */
static const char *tasks_due_as_subtitle(const char *due) {
  return (due && due[0]) ? due : NULL;
}

static int tasks_completed_nav_row_index(void) {
  return s_tasks_show_completed_nav ? (1 + s_tasks_open_count) : -1;
}

static uint16_t tasks_get_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  (void)ml;
  (void)section;
  (void)ctx;
  return s_tasks_num_rows > 0 ? (uint16_t)s_tasks_num_rows : 1;
}

static void tasks_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *idx, void *cb_ctx) {
  (void)cb_ctx;
  if (idx->row >= s_tasks_num_rows) {
    return;
  }
  if (idx->row == 0) {
    menu_cell_basic_draw(ctx, cell_layer, "Add task", NULL, (GBitmap *)(void *)ui_assets_add_task());
    return;
  }
  int nav = tasks_completed_nav_row_index();
  if (nav >= 0 && idx->row == nav) {
    menu_cell_basic_draw(ctx, cell_layer, "Completed", NULL, (GBitmap *)(void *)ui_assets_completed());
    return;
  }
  if (!s_tasks_titles[idx->row]) {
    return;
  }
  ui_menu_wrap_cell_draw(ctx, cell_layer, s_tasks_menu, idx, s_tasks_titles[idx->row],
                           tasks_due_as_subtitle(s_tasks_due[idx->row]), NULL);
}

static int16_t tasks_get_height(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ctx;
  if (idx->row == 0) {
    return (int16_t)UI_CELL_MIN_HEIGHT;
  }
  int nav = tasks_completed_nav_row_index();
  if (nav >= 0 && idx->row == nav) {
    return (int16_t)UI_CELL_MIN_HEIGHT;
  }
  if (idx->row >= s_tasks_num_rows || !s_tasks_titles[idx->row]) {
    return ui_menu_wrap_cell_measure_height(ml, " ", NULL);
  }
  return ui_menu_wrap_cell_measure_height(ml, s_tasks_titles[idx->row],
                                          tasks_due_as_subtitle(s_tasks_due[idx->row]));
}

static void tasks_select(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ml;
  (void)ctx;
  if (s_tasks_suppress_next_select_click) {
    s_tasks_suppress_next_select_click = false;
    return;
  }
  int row = idx->row;
  if (row == 0) {
    main_menu_set_adding_list(false);
    if (pebble_tasks_dictation_available()) {
      dictation_session_start(pebble_tasks_dictation_session());
    } else {
      ui_toast_show("Voice input off");
    }
    return;
  }
  int navRow = tasks_completed_nav_row_index();
  if (navRow >= 0 && row == navRow) {
    completed_menu_push(s_current_list_index);
    return;
  }
  if (row > 0 && row <= s_tasks_open_count) {
    Layer *r = window_get_root_layer(s_tasks_window);
    ui_loading_start(r);
    messaging_send(CMD_W_COMPLETE, s_current_list_index, row - 1, NULL);
  }
}

static void tasks_select_long(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ml;
  (void)ctx;
  int row = idx->row;
  if (row == 0) {
    return;
  }
  int navRow = tasks_completed_nav_row_index();
  if (navRow >= 0 && row == navRow) {
    return;
  }
  if (row > 0 && row <= s_tasks_open_count) {
    s_tasks_suppress_next_select_click = true;
    task_action_menu_show(s_current_list_index, row - 1, s_tasks_titles[row], s_tasks_due[row],
                          s_tasks_due_iso[row]);
  }
}

static void setup_tasks_menu_ui(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect wb = layer_get_bounds(window_layer);

#ifdef PBL_ROUND
  GRect menu_bounds = wb;
#else
  GRect menu_bounds = GRect(0, UI_CLOCK_BAR_HEIGHT, wb.size.w, wb.size.h - UI_CLOCK_BAR_HEIGHT);
#endif

  window_set_background_color(window, theme_bg());

  s_tasks_menu = menu_layer_create(menu_bounds);
  menu_layer_set_click_config_onto_window(s_tasks_menu, window);
  menu_layer_set_center_focused(s_tasks_menu, PBL_IF_ROUND_ELSE(true, false));
  menu_layer_set_callbacks(
      s_tasks_menu, NULL,
      (MenuLayerCallbacks){
          .get_num_rows = tasks_get_num_rows,
          .draw_row = tasks_draw_row,
          .get_cell_height = tasks_get_height,
          .select_click = tasks_select,
          .select_long_click = tasks_select_long,
      });
  menu_layer_set_normal_colors(s_tasks_menu, theme_bg(), theme_text());
  menu_layer_set_highlight_colors(s_tasks_menu, theme_menu_highlight_bg(), theme_menu_highlight_text());
  menu_layer_configure_scroll_behavior(s_tasks_menu);

  layer_add_child(window_layer, menu_layer_get_layer(s_tasks_menu));

#ifndef PBL_ROUND
  s_tasks_clock = ui_clock_bar_create_for_window_size(wb.size);
  layer_add_child(window_layer, s_tasks_clock);
#endif
}

static void destroy_tasks_menu_layers(void) {
  if (s_tasks_menu) {
    menu_layer_destroy(s_tasks_menu);
    s_tasks_menu = NULL;
  }
#ifndef PBL_ROUND
  if (s_tasks_clock) {
    ui_clock_bar_unlink_and_destroy(s_tasks_clock);
    s_tasks_clock = NULL;
  }
#endif
}

#ifdef PBL_TOUCH
static void tasks_menu_window_appear(Window *w) {
  (void)w;
  if (!s_tasks_menu) {
    menu_layer_touch_on_window_disappear();
    return;
  }
  MenuLayerTouchHooks hooks = {.menu = s_tasks_menu,
                              .callback_context = NULL,
                              .get_num_rows = tasks_get_num_rows,
                              .get_cell_height = tasks_get_height,
                              .select_click = tasks_select};
  menu_layer_touch_on_window_appear(&hooks);
}

static void tasks_menu_window_disappear(Window *w) {
  (void)w;
  menu_layer_touch_on_window_disappear();
}
#endif

static void destroy_tasks_menu_data(void) {
  int open = s_tasks_open_count;
  for (int i = 1; i <= MAX_MENU_LINES; i++) {
    if (s_tasks_due[i]) {
      free(s_tasks_due[i]);
      s_tasks_due[i] = NULL;
    }
    if (s_tasks_due_iso[i]) {
      free(s_tasks_due_iso[i]);
      s_tasks_due_iso[i] = NULL;
    }
  }
  str_util_free_titles(s_tasks_titles, 1 + open);
  s_tasks_num_rows = 0;
  s_tasks_open_count = 0;
  s_tasks_show_completed_nav = false;
}

static void rebuild_tasks_menu_internal(const char *payload, int for_list, bool has_completed) {
  (void)for_list;
  char buf[TEXT_BUF];
  destroy_tasks_menu_data();

  if (payload) {
    strncpy(buf, payload, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
  } else {
    buf[0] = '\0';
  }
  s_tasks_open_count = str_util_split_lines(buf, s_tasks_titles + 1, MAX_MENU_LINES);
  s_tasks_show_completed_nav = has_completed;
  s_tasks_num_rows = 1 + s_tasks_open_count + (s_tasks_show_completed_nav ? 1 : 0);
  s_tasks_titles[0] = str_util_strdup("");
  if (!s_tasks_titles[0]) {
    s_tasks_num_rows = 0;
    return;
  }
  for (int i = 1; i <= s_tasks_open_count; i++) {
    s_tasks_due[i] = NULL;
    s_tasks_due_iso[i] = NULL;
    char *line = s_tasks_titles[i];
    if (!line) {
      continue;
    }
    char *p = strchr(line, '\x1F');
    if (p) {
      *p = '\0';
      char *rest = p + 1;
      char *p2 = strchr(rest, '\x1F');
      if (p2) {
        *p2 = '\0';
        s_tasks_due_iso[i] = str_util_strdup(rest);
        s_tasks_due[i] = str_util_strdup(p2 + 1);
      } else {
        s_tasks_due[i] = str_util_strdup(rest);
      }
    }
  }
  if (s_tasks_menu) {
    menu_layer_reload_data(s_tasks_menu);
  }
}

void tasks_menu_reload_from_payload_if_visible(const char *payload, int list_index,
                                               bool has_completed) {
  if (s_tasks_window_is_on_stack && list_index == s_current_list_index) {
    rebuild_tasks_menu_internal(payload, list_index, has_completed);
  }
}

void tasks_menu_push(int list_index) {
  s_current_list_index = list_index;
  if (!s_tasks_window) {
    s_tasks_window = window_create();
    window_set_window_handlers(s_tasks_window, (WindowHandlers){
                                                    .load = tasks_menu_window_load,
                                                    .unload = tasks_menu_window_unload,
#ifdef PBL_TOUCH
                                                    .appear = tasks_menu_window_appear,
                                                    .disappear = tasks_menu_window_disappear,
#endif
                                                });
  }
  s_tasks_window_is_on_stack = true;
  window_stack_push(s_tasks_window, true);
}

int tasks_menu_current_list_index(void) { return s_current_list_index; }

void tasks_menu_cancel_select_suppress(void) { s_tasks_suppress_next_select_click = false; }

void tasks_menu_window_load(Window *w) {
  (void)w;
  if (!s_tasks_menu) {
    setup_tasks_menu_ui(s_tasks_window);
    destroy_tasks_menu_data();
    s_tasks_titles[0] = str_util_strdup("");
    s_tasks_num_rows = 1;
    s_tasks_open_count = 0;
    s_tasks_show_completed_nav = false;
    menu_layer_reload_data(s_tasks_menu);
  }
  {
    Layer *r = window_get_root_layer(s_tasks_window);
    ui_loading_start(r);
  }
  messaging_request_open_for_list(s_current_list_index);
}

void tasks_menu_window_unload(Window *w) {
  (void)w;
  s_tasks_window_is_on_stack = false;
  ui_loading_stop();
  ui_toast_detach_from_window(s_tasks_window);
  destroy_tasks_menu_layers();
  destroy_tasks_menu_data();
}

void tasks_menu_apply_theme(void) {
  if (!s_tasks_window) {
    return;
  }
  window_set_background_color(s_tasks_window, theme_bg());
  if (s_tasks_menu) {
    menu_layer_set_normal_colors(s_tasks_menu, theme_bg(), theme_text());
    menu_layer_set_highlight_colors(s_tasks_menu, theme_menu_highlight_bg(), theme_menu_highlight_text());
    menu_layer_reload_data(s_tasks_menu);
  }
#ifndef PBL_ROUND
  ui_clock_bar_invalidate(s_tasks_clock);
#endif
}

void tasks_menu_deinit(void) {
  if (s_tasks_window) {
    window_destroy(s_tasks_window);
    s_tasks_window = NULL;
  }
}
