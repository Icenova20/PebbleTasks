#include "tasks_menu.h"
#include "completed_menu.h"
#include "main_menu.h"
#include "messaging.h"
#include "pebble_tasks.h"
#include "protocol.h"
#include "str_util.h"
#include "theme.h"
#include "ui_assets.h"
#include "ui_clock_bar.h"
#include "ui_constants.h"
#include "ui_draw.h"
#include "ui_loading.h"
#include "ui_toast.h"

static Window *s_tasks_window;
static MenuLayer *s_tasks_menu;
#ifndef PBL_ROUND
static Layer *s_tasks_clock;
#endif
static GTextAttributes *s_tasks_text_attr;
static int s_tasks_num_rows;
static char *s_tasks_titles[MAX_MENU_LINES + 1];
static int s_tasks_open_count;
static bool s_tasks_show_completed_nav;
static int s_current_list_index;
static bool s_tasks_window_is_on_stack;

static bool s_tasks_suppress_next_select_click;

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
    ui_draw_add_labeled_row(ctx, cell_layer, false, s_tasks_text_attr);
    return;
  }
  int nav = tasks_completed_nav_row_index();
  if (nav >= 0 && idx->row == nav) {
    ui_draw_menu_leading_icon_row(ctx, cell_layer, ui_assets_completed(), "Completed", false, s_tasks_text_attr);
    return;
  }
  if (!s_tasks_titles[idx->row]) {
    return;
  }
  ui_draw_tasks_open_cell(ctx, cell_layer, s_tasks_titles[idx->row], s_tasks_text_attr);
}

static int16_t tasks_get_height(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ml;
  (void)ctx;
  if (idx->row == 0) {
    return (int16_t)UI_ADD_ROW_CELL_H;
  }
  int nav = tasks_completed_nav_row_index();
  if (nav >= 0 && idx->row == nav) {
    return UI_CELL_MIN;
  }
  if (idx->row >= s_tasks_num_rows || !s_tasks_titles[idx->row]) {
    return UI_CELL_MIN;
  }
  Layer *wl = window_get_root_layer(s_tasks_window);
  int w = ui_draw_task_row_text_layout_width(wl);
  GSize sz = graphics_text_layout_get_content_size_with_attributes(
      s_tasks_titles[idx->row], fonts_get_system_font(UI_MENU_FONT_KEY),
      GRect(0, 0, w, 500), GTextOverflowModeTrailingEllipsis,
      PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft), NULL);
  int h = sz.h + UI_CELL_MARGIN * 2;
  if (h < UI_CELL_MIN) {
    h = UI_CELL_MIN;
  }
  if (h > UI_CELL_MAX) {
    h = UI_CELL_MAX;
  }
  return (int16_t)h;
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
  s_tasks_suppress_next_select_click = true;
  int row = idx->row;
  if (row == 0) {
    return;
  }
  int navRow = tasks_completed_nav_row_index();
  if (navRow >= 0 && row == navRow) {
    return;
  }
  if (row > 0 && row <= s_tasks_open_count) {
    messaging_send(CMD_W_DELETE_TASK, s_current_list_index, row - 1, NULL);
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

  s_tasks_text_attr = graphics_text_attributes_create();
#ifdef PBL_ROUND
  graphics_text_attributes_enable_screen_text_flow(s_tasks_text_attr, UI_CELL_MARGIN * 2);
#endif

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
  menu_layer_set_highlight_colors(s_tasks_menu, theme_highlight_bg(), theme_highlight_text());

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
  if (s_tasks_text_attr) {
    graphics_text_attributes_destroy(s_tasks_text_attr);
    s_tasks_text_attr = NULL;
  }
}

static void destroy_tasks_menu_data(void) {
  str_util_free_titles(s_tasks_titles, s_tasks_num_rows);
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
                                                });
  }
  s_tasks_window_is_on_stack = true;
  window_stack_push(s_tasks_window, true);
}

int tasks_menu_current_list_index(void) { return s_current_list_index; }

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
    menu_layer_set_highlight_colors(s_tasks_menu, theme_highlight_bg(), theme_highlight_text());
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
