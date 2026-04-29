#include "main_menu.h"
#include "messaging.h"
#include "pebble_tasks.h"
#include "protocol.h"
#include "str_util.h"
#include "tasks_menu.h"
#include "theme.h"
#include "ui_clock_bar.h"
#include "ui_constants.h"
#include "ui_draw.h"
#include "ui_loading.h"
#include "ui_toast.h"

static Window *s_main_window;
static MenuLayer *s_main_menu;
#ifndef PBL_ROUND
static Layer *s_main_clock;
#endif
static GTextAttributes *s_main_text_attr;
static int s_main_num_rows;
static char *s_main_titles[MAX_MENU_LINES + 1];
static int s_main_real_list_count;

static bool s_main_suppress_next_select_click;
static bool s_adding_list;

bool main_menu_is_adding_list(void) { return s_adding_list; }

void main_menu_set_adding_list(bool add_list) { s_adding_list = add_list; }

static uint16_t main_get_num_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  (void)ml;
  (void)section;
  (void)ctx;
  return s_main_num_rows > 0 ? (uint16_t)s_main_num_rows : 1;
}

static void main_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *idx, void *cb_ctx) {
  (void)cb_ctx;
  if (idx->row >= s_main_num_rows) {
    return;
  }
  if (idx->row == 0) {
    ui_draw_add_labeled_row(ctx, cell_layer, true, s_main_text_attr);
    return;
  }
  if (!s_main_titles[idx->row]) {
    return;
  }
  ui_draw_menu_title_row(ctx, cell_layer, s_main_titles[idx->row], false, s_main_text_attr);
}

static int16_t main_get_height(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ml;
  (void)ctx;
  if (idx->row == 0) {
    return (int16_t)UI_ADD_ROW_CELL_H;
  }
  if (idx->row >= s_main_num_rows || !s_main_titles[idx->row]) {
    return UI_CELL_MIN;
  }
  Layer *wl = window_get_root_layer(s_main_window);
  int w = ui_draw_text_cell_width(wl);
  GSize sz = graphics_text_layout_get_content_size_with_attributes(
      s_main_titles[idx->row], fonts_get_system_font(UI_MENU_FONT_KEY),
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

static void main_select(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ml;
  (void)ctx;
  if (s_main_suppress_next_select_click) {
    s_main_suppress_next_select_click = false;
    return;
  }
  int row = idx->row;
  if (row == 0) {
    main_menu_set_adding_list(true);
    if (pebble_tasks_dictation_available()) {
      dictation_session_start(pebble_tasks_dictation_session());
    } else {
      ui_toast_show("Voice input off");
    }
    return;
  }
  if (row > 0 && row <= s_main_real_list_count) {
    tasks_menu_push(row - 1);
  }
}

static void main_select_long(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ml;
  (void)ctx;
  s_main_suppress_next_select_click = true;
  int row = idx->row;
  if (row == 0) {
    return;
  }
  if (row > 0 && row <= s_main_real_list_count) {
    messaging_send(CMD_W_DELETE_LIST, row - 1, -1, NULL);
  }
}

static void destroy_main_menu_layers(void) {
  if (s_main_menu) {
    menu_layer_destroy(s_main_menu);
    s_main_menu = NULL;
  }
#ifndef PBL_ROUND
  if (s_main_clock) {
    ui_clock_bar_unlink_and_destroy(s_main_clock);
    s_main_clock = NULL;
  }
#endif
  if (s_main_text_attr) {
    graphics_text_attributes_destroy(s_main_text_attr);
    s_main_text_attr = NULL;
  }
}

static void destroy_main_menu_data(void) {
  str_util_free_titles(s_main_titles, s_main_num_rows);
  s_main_num_rows = 0;
  s_main_real_list_count = 0;
}

static void setup_main_menu_ui(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect wb = layer_get_bounds(window_layer);

#ifdef PBL_ROUND
  GRect menu_bounds = wb;
#else
  GRect menu_bounds = GRect(0, UI_CLOCK_BAR_HEIGHT, wb.size.w, wb.size.h - UI_CLOCK_BAR_HEIGHT);
#endif

  window_set_background_color(window, theme_bg());

  s_main_text_attr = graphics_text_attributes_create();
#ifdef PBL_ROUND
  graphics_text_attributes_enable_screen_text_flow(s_main_text_attr, UI_CELL_MARGIN * 2);
#endif

  s_main_menu = menu_layer_create(menu_bounds);
  menu_layer_set_click_config_onto_window(s_main_menu, window);
  menu_layer_set_center_focused(s_main_menu, PBL_IF_ROUND_ELSE(true, false));
  menu_layer_set_callbacks(
      s_main_menu, NULL,
      (MenuLayerCallbacks){
          .get_num_rows = main_get_num_rows,
          .draw_row = main_draw_row,
          .get_cell_height = main_get_height,
          .select_click = main_select,
          .select_long_click = main_select_long,
      });
  menu_layer_set_normal_colors(s_main_menu, theme_bg(), theme_text());
  menu_layer_set_highlight_colors(s_main_menu, theme_highlight_bg(), theme_highlight_text());

  layer_add_child(window_layer, menu_layer_get_layer(s_main_menu));

#ifndef PBL_ROUND
  s_main_clock = ui_clock_bar_create_for_window_size(wb.size);
  layer_add_child(window_layer, s_main_clock);
#endif
}

void main_menu_reload_from_payload(const char *payload) {
  char buf[TEXT_BUF];
  destroy_main_menu_data();

  if (payload) {
    strncpy(buf, payload, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
  } else {
    buf[0] = '\0';
  }
  s_main_real_list_count = str_util_split_lines(buf, s_main_titles + 1, MAX_MENU_LINES);
  s_main_num_rows = 1 + s_main_real_list_count;
  s_main_titles[0] = str_util_strdup("");
  if (!s_main_titles[0]) {
    s_main_num_rows = 0;
    return;
  }
  if (s_main_menu) {
    menu_layer_reload_data(s_main_menu);
  }
}

void main_menu_window_load(Window *w) {
  (void)w;
  Layer *root = window_get_root_layer(s_main_window);
  setup_main_menu_ui(s_main_window);
  ui_toast_init(root);
  main_menu_reload_from_payload("");
  ui_loading_start(root);
  messaging_request_lists();
}

void main_menu_window_unload(Window *w) {
  (void)w;
  ui_loading_stop();
  destroy_main_menu_layers();
  destroy_main_menu_data();
  ui_toast_deinit();
}

void main_menu_init(void) {
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers){
                                               .load = main_menu_window_load,
                                               .unload = main_menu_window_unload,
                                           });
}

void main_menu_apply_theme(void) {
  if (!s_main_window) {
    return;
  }
  window_set_background_color(s_main_window, theme_bg());
  if (s_main_menu) {
    menu_layer_set_normal_colors(s_main_menu, theme_bg(), theme_text());
    menu_layer_set_highlight_colors(s_main_menu, theme_highlight_bg(), theme_highlight_text());
    menu_layer_reload_data(s_main_menu);
  }
#ifndef PBL_ROUND
  ui_clock_bar_invalidate(s_main_clock);
#endif
}

void main_menu_deinit(void) {
  if (s_main_window) {
    window_destroy(s_main_window);
    s_main_window = NULL;
  }
}

Window *main_menu_get_window(void) { return s_main_window; }
