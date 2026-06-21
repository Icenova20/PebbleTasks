#include "main_menu.h"
#include "list_action_menu.h"
#include "messaging.h"
#include "pebble_tasks.h"
#include "protocol.h"
#include "str_util.h"
#include "tasks_menu.h"
#include "theme.h"
#include "ui_assets.h"
#include "ui_clock_bar.h"
#include "ui_constants.h"
#include "ui_menu_wrap_cell.h"
#include "ui_loading.h"
#include "ui_toast.h"
#include "menu_layer_touch_support.h"

static Window *s_main_window;
static MenuLayer *s_main_menu;
#ifndef PBL_ROUND
static Layer *s_main_clock;
#endif
static int s_main_num_rows;
static char *s_main_titles[MAX_MENU_LINES + 1];
static int s_main_real_list_count;

static bool s_main_suppress_next_select_click;
static bool s_adding_list;
static bool s_has_auto_opened = false;

/** PKJS may attach appmessage after the first outbound CMD_W_ASK_LISTS; retry lists shortly after load. */
static AppTimer *s_lists_retry_timer;

static void main_menu_delayed_lists_cb(void *data) {
  (void)data;
  s_lists_retry_timer = NULL;
  messaging_request_lists();
}

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
    menu_cell_basic_draw(ctx, cell_layer, "Add list", NULL, (GBitmap *)(void *)ui_assets_add_list());
    return;
  }
  if (!s_main_titles[idx->row]) {
    return;
  }
  ui_menu_wrap_cell_draw(ctx, cell_layer, s_main_menu, idx, s_main_titles[idx->row], NULL, NULL);
}

static int16_t main_get_height(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ctx;
  if (idx->row == 0) {
    return (int16_t)UI_CELL_MIN_HEIGHT;
  }
  if (idx->row >= s_main_num_rows || !s_main_titles[idx->row]) {
    return ui_menu_wrap_cell_measure_height(ml, " ", NULL);
  }
  return ui_menu_wrap_cell_measure_height(ml, s_main_titles[idx->row], NULL);
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
  int row = idx->row;
  if (row == 0) {
    return;
  }
  if (row > 0 && row <= s_main_real_list_count) {
    s_main_suppress_next_select_click = true;
    list_action_menu_show(row - 1, s_main_titles[row]);
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
}

static void destroy_main_menu_data(void) {
  str_util_free_titles(s_main_titles, s_main_num_rows);
  s_main_num_rows = 0;
  s_main_real_list_count = 0;
}

#ifdef PBL_TOUCH
static void main_menu_window_appear(Window *w) {
  (void)w;
  if (!s_main_menu) {
    menu_layer_touch_on_window_disappear();
    return;
  }
  MenuLayerTouchHooks hooks = {.menu = s_main_menu,
                              .callback_context = NULL,
                              .get_num_rows = main_get_num_rows,
                              .get_cell_height = main_get_height,
                              .select_click = main_select};
  menu_layer_touch_on_window_appear(&hooks);
}

static void main_menu_window_disappear(Window *w) {
  (void)w;
  menu_layer_touch_on_window_disappear();
}
#endif

static void setup_main_menu_ui(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect wb = layer_get_bounds(window_layer);

#ifdef PBL_ROUND
  GRect menu_bounds = wb;
#else
  GRect menu_bounds = GRect(0, UI_CLOCK_BAR_HEIGHT, wb.size.w, wb.size.h - UI_CLOCK_BAR_HEIGHT);
#endif

  window_set_background_color(window, theme_bg());

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
  menu_layer_set_highlight_colors(s_main_menu, theme_menu_highlight_bg(), theme_menu_highlight_text());
  menu_layer_configure_scroll_behavior(s_main_menu);

  layer_add_child(window_layer, menu_layer_get_layer(s_main_menu));

#ifndef PBL_ROUND
  s_main_clock = ui_clock_bar_create_for_window_size(wb.size);
  layer_add_child(window_layer, s_main_clock);
#endif
}

void main_menu_cancel_select_suppress(void) { s_main_suppress_next_select_click = false; }

void main_menu_reload_from_payload(const char *payload) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "reload start");
  char *buf = malloc(TEXT_BUF);
  if (!buf) return;
  
  destroy_main_menu_data();

  if (payload) {
    strncpy(buf, payload, TEXT_BUF - 1);
    buf[TEXT_BUF - 1] = '\0';
  } else {
    buf[0] = '\0';
  }
  s_main_real_list_count = str_util_split_lines(buf, s_main_titles + 1, MAX_MENU_LINES);
  s_main_num_rows = 1 + s_main_real_list_count;
  s_main_titles[0] = str_util_strdup("");
  if (!s_main_titles[0]) {
    s_main_num_rows = 0;
    free(buf);
    return;
  }
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "reload before menu reload");
  if (s_main_menu) {
    menu_layer_reload_data(s_main_menu);
  }

  APP_LOG(APP_LOG_LEVEL_DEBUG, "reload before persist");
  if (!s_has_auto_opened && persist_exists(PERSIST_KEY_DEFAULT_LIST_TITLE)) {
    char *default_title = malloc(TEXT_BUF);
    if (default_title) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "reload reading persist");
      persist_read_string(PERSIST_KEY_DEFAULT_LIST_TITLE, default_title, TEXT_BUF);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "reload checking titles");
      for (int i = 1; i <= s_main_real_list_count; i++) {
        if (s_main_titles[i] && strcmp(s_main_titles[i], default_title) == 0) {
          tasks_menu_push(i - 1);
          break;
        }
      }
      free(default_title);
    }
    s_has_auto_opened = true;
  }
  free(buf);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "reload end");
}

void main_menu_window_load(Window *w) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "main_menu_window_load start");
  (void)w;
  Layer *root = window_get_root_layer(s_main_window);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "setup_main_menu_ui");
  setup_main_menu_ui(s_main_window);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "ui_toast_init");
  ui_toast_init(root);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "main_menu_reload");
  main_menu_reload_from_payload("");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "ui_loading");
  ui_loading_start(root);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "messaging");
  messaging_request_lists();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "main_menu_window_load end");

  if (s_lists_retry_timer) {
    app_timer_cancel(s_lists_retry_timer);
    s_lists_retry_timer = NULL;
  }
  s_lists_retry_timer = app_timer_register(450, main_menu_delayed_lists_cb, NULL);
}

void main_menu_window_unload(Window *w) {
  (void)w;
  if (s_lists_retry_timer) {
    app_timer_cancel(s_lists_retry_timer);
    s_lists_retry_timer = NULL;
  }
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
#ifdef PBL_TOUCH
                                               .appear = main_menu_window_appear,
                                               .disappear = main_menu_window_disappear,
#endif
                                           });
}

void main_menu_apply_theme(void) {
  if (!s_main_window) {
    return;
  }
  window_set_background_color(s_main_window, theme_bg());
  if (s_main_menu) {
    menu_layer_set_normal_colors(s_main_menu, theme_bg(), theme_text());
    menu_layer_set_highlight_colors(s_main_menu, theme_menu_highlight_bg(), theme_menu_highlight_text());
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
