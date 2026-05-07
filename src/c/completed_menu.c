#include "completed_menu.h"

#include "messaging.h"
#include "protocol.h"
#include "str_util.h"
#include "ui_assets.h"
#include "theme.h"
#include "ui_clock_bar.h"
#include "ui_constants.h"
#include "ui_menu_wrap_cell.h"
#include "ui_loading.h"
#include "ui_toast.h"

static Window *s_completed_window;
static MenuLayer *s_completed_menu;
#ifndef PBL_ROUND
static Layer *s_completed_clock;
#endif
static int s_completed_num_rows;
static char *s_completed_titles[MAX_MENU_LINES + 1];
static int s_completed_line_count;
static int s_completed_list_index;
static bool s_completed_window_on_stack;

static bool s_completed_suppress_next_select_click;

static uint16_t completed_get_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  (void)ml;
  (void)section;
  (void)ctx;
  return s_completed_num_rows > 0 ? (uint16_t)s_completed_num_rows : 1;
}

static void completed_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *idx, void *cb_ctx) {
  (void)cb_ctx;
  if (idx->row >= s_completed_num_rows) {
    return;
  }
  if (idx->row == 0) {
    menu_cell_basic_draw(ctx, cell_layer, "Clear all", NULL, (GBitmap *)(void *)ui_assets_trash());
    return;
  }
  if (!s_completed_titles[idx->row]) {
    return;
  }
  ui_menu_wrap_cell_draw(ctx, cell_layer, s_completed_menu, idx, s_completed_titles[idx->row], NULL, NULL);
}

static int16_t completed_get_height(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ctx;
  if (idx->row == 0) {
    return (int16_t)UI_CELL_MIN_HEIGHT;
  }
  if (idx->row >= s_completed_num_rows || !s_completed_titles[idx->row]) {
    return ui_menu_wrap_cell_measure_height(ml, " ", NULL);
  }
  return ui_menu_wrap_cell_measure_height(ml, s_completed_titles[idx->row], NULL);
}

static void completed_select(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ml;
  (void)ctx;
  if (s_completed_suppress_next_select_click) {
    s_completed_suppress_next_select_click = false;
    return;
  }
  int row = idx->row;
  Layer *r = window_get_root_layer(s_completed_window);
  if (row == 0) {
    ui_loading_start(r);
    messaging_send(CMD_W_CLEAR_COMPLETED, s_completed_list_index, -1, NULL);
    return;
  }
  ui_loading_start(r);
  messaging_send(CMD_W_UNCOMPLETE_COMPLETED, s_completed_list_index, row - 1, NULL);
}

static void completed_select_long(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ml;
  (void)ctx;
  int row = idx->row;
  if (row == 0) {
    return;
  }
  if (row > 0 && row < s_completed_num_rows) {
    s_completed_suppress_next_select_click = true;
    Layer *r = window_get_root_layer(s_completed_window);
    ui_loading_start(r);
    messaging_send(CMD_W_DELETE_COMPLETED_TASK, s_completed_list_index, row - 1, NULL);
  }
}

static void setup_completed_menu_ui(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect wb = layer_get_bounds(window_layer);

#ifdef PBL_ROUND
  GRect menu_bounds = wb;
#else
  GRect menu_bounds = GRect(0, UI_CLOCK_BAR_HEIGHT, wb.size.w, wb.size.h - UI_CLOCK_BAR_HEIGHT);
#endif

  window_set_background_color(window, theme_bg());

  s_completed_menu = menu_layer_create(menu_bounds);
  menu_layer_set_click_config_onto_window(s_completed_menu, window);
  menu_layer_set_center_focused(s_completed_menu, PBL_IF_ROUND_ELSE(true, false));
  menu_layer_set_callbacks(
      s_completed_menu, NULL,
      (MenuLayerCallbacks){
          .get_num_rows = completed_get_num_rows,
          .draw_row = completed_draw_row,
          .get_cell_height = completed_get_height,
          .select_click = completed_select,
          .select_long_click = completed_select_long,
      });
  menu_layer_set_normal_colors(s_completed_menu, theme_bg(), theme_text());
  menu_layer_set_highlight_colors(s_completed_menu, theme_menu_highlight_bg(), theme_menu_highlight_text());

  layer_add_child(window_layer, menu_layer_get_layer(s_completed_menu));

#ifndef PBL_ROUND
  s_completed_clock = ui_clock_bar_create_for_window_size(wb.size);
  layer_add_child(window_layer, s_completed_clock);
#endif
}

static void destroy_completed_menu_layers(void) {
  if (s_completed_menu) {
    menu_layer_destroy(s_completed_menu);
    s_completed_menu = NULL;
  }
#ifndef PBL_ROUND
  if (s_completed_clock) {
    ui_clock_bar_unlink_and_destroy(s_completed_clock);
    s_completed_clock = NULL;
  }
#endif
}

static void destroy_completed_menu_data(void) {
  str_util_free_titles(s_completed_titles + 1, s_completed_line_count);
  s_completed_line_count = 0;
  s_completed_num_rows = 0;
}

static void rebuild_completed_menu_internal(const char *payload) {
  char buf[TEXT_BUF];
  destroy_completed_menu_data();

  if (payload) {
    strncpy(buf, payload, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
  } else {
    buf[0] = '\0';
  }

  int max_lines = MAX_MENU_LINES > 0 ? MAX_MENU_LINES - 1 : 0;
  if (max_lines < 1) {
    max_lines = 1;
  }
  s_completed_line_count = str_util_split_lines(buf, s_completed_titles + 1, max_lines);
  s_completed_num_rows = 1 + s_completed_line_count;

  if (s_completed_menu) {
    menu_layer_reload_data(s_completed_menu);
  }
}

void completed_menu_reload_from_payload_if_visible(const char *payload, int list_index) {
  if (s_completed_window_on_stack && list_index == s_completed_list_index) {
    rebuild_completed_menu_internal(payload);
  }
}

void completed_menu_push(int list_index) {
  s_completed_list_index = list_index;
  if (!s_completed_window) {
    s_completed_window = window_create();
    window_set_window_handlers(s_completed_window, (WindowHandlers){
                                                   .load = completed_menu_window_load,
                                                   .unload = completed_menu_window_unload,
                                               });
  }
  s_completed_window_on_stack = true;
  window_stack_push(s_completed_window, true);
}

void completed_menu_window_load(Window *w) {
  (void)w;
  if (!s_completed_menu) {
    setup_completed_menu_ui(s_completed_window);
    destroy_completed_menu_data();
    s_completed_num_rows = 1;
    menu_layer_reload_data(s_completed_menu);
  }
  {
    Layer *r = window_get_root_layer(s_completed_window);
    ui_loading_start(r);
  }
  messaging_request_completed_for_list(s_completed_list_index);
}

void completed_menu_window_unload(Window *w) {
  (void)w;
  s_completed_window_on_stack = false;
  ui_loading_stop();
  ui_toast_detach_from_window(s_completed_window);
  destroy_completed_menu_layers();
  destroy_completed_menu_data();
}

void completed_menu_apply_theme(void) {
  if (!s_completed_window) {
    return;
  }
  window_set_background_color(s_completed_window, theme_bg());
  if (s_completed_menu) {
    menu_layer_set_normal_colors(s_completed_menu, theme_bg(), theme_text());
    menu_layer_set_highlight_colors(s_completed_menu, theme_menu_highlight_bg(), theme_menu_highlight_text());
    menu_layer_reload_data(s_completed_menu);
  }
#ifndef PBL_ROUND
  ui_clock_bar_invalidate(s_completed_clock);
#endif
}

void completed_menu_deinit(void) {
  if (s_completed_window) {
    window_destroy(s_completed_window);
    s_completed_window = NULL;
  }
}
