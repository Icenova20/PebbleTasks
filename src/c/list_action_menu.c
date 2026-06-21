#include "list_action_menu.h"

#include "main_menu.h"
#include "messaging.h"
#include "protocol.h"
#include "str_util.h"
#include "theme.h"
#include "ui_clock_bar.h"
#include "ui_constants.h"
#include "ui_toast.h"

#include <pebble.h>
#include <stdlib.h>

#include "menu_layer_touch_support.h"

static Window *s_win;
static MenuLayer *s_menu;
#ifndef PBL_ROUND
static TextLayer *s_rect_title;
#endif
static int s_list_ix;
static char *s_saved_title;

static void clear_saved_strings(void) {
  free(s_saved_title);
  s_saved_title = NULL;
}

bool is_list_timeline_synced(const char *title) {
  if (!title || !title[0]) return false;
  char synced_lists[256] = {0};
  if (persist_exists(PERSIST_KEY_TIMELINE_SYNC_LISTS)) {
    persist_read_string(PERSIST_KEY_TIMELINE_SYNC_LISTS, synced_lists, sizeof(synced_lists));
  }
  char *p = synced_lists;
  while (p && *p) {
    char *next = strchr(p, '\n');
    if (next) *next = '\0';
    if (strcmp(p, title) == 0) {
      if (next) *next = '\n';
      return true;
    }
    if (next) {
      *next = '\n';
      p = next + 1;
    } else {
      break;
    }
  }
  return false;
}

static void set_list_timeline_synced(const char *title, bool sync) {
  if (!title || !title[0]) return;
  char synced_lists[256] = {0};
  if (persist_exists(PERSIST_KEY_TIMELINE_SYNC_LISTS)) {
    persist_read_string(PERSIST_KEY_TIMELINE_SYNC_LISTS, synced_lists, sizeof(synced_lists));
  }
  char new_synced[256] = {0};
  char *p = synced_lists;
  while (p && *p) {
    char *next = strchr(p, '\n');
    if (next) *next = '\0';
    if (strcmp(p, title) != 0 && p[0] != '\0') {
      if (new_synced[0] != '\0') {
        snprintf(new_synced + strlen(new_synced), sizeof(new_synced) - strlen(new_synced), "\n%s", p);
      } else {
        snprintf(new_synced, sizeof(new_synced), "%s", p);
      }
    }
    if (next) {
      *next = '\n';
      p = next + 1;
    } else {
      break;
    }
  }
  if (sync) {
    if (new_synced[0] != '\0') {
      snprintf(new_synced + strlen(new_synced), sizeof(new_synced) - strlen(new_synced), "\n%s", title);
    } else {
      snprintf(new_synced, sizeof(new_synced), "%s", title);
    }
  }
  persist_write_string(PERSIST_KEY_TIMELINE_SYNC_LISTS, new_synced);
}

static uint16_t list_action_get_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  (void)ml;
  (void)section;
  (void)ctx;
  return 3;
}

static void list_action_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *idx, void *cb_ctx) {
  (void)cb_ctx;
  if (idx->row == 0) {
    menu_cell_basic_draw(ctx, cell_layer, "Delete list", NULL, NULL);
  } else if (idx->row == 1) {
    menu_cell_basic_draw(ctx, cell_layer, "Set as Default", NULL, NULL);
  } else if (idx->row == 2) {
    bool is_synced = false;
    if (s_saved_title) {
      is_synced = is_list_timeline_synced(s_saved_title);
    }
    menu_cell_basic_draw(ctx, cell_layer, "Timeline sync", is_synced ? "On" : "Off", NULL);
  }
}

static int16_t list_action_get_cell_height(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ctx;
#ifndef PBL_ROUND
  (void)ml;
  (void)idx;
  return UI_CELL_MIN_HEIGHT;
#else
  MenuIndex sel = menu_layer_get_selected_index(ml);
  return menu_index_compare(idx, &sel) == 0 ? MENU_CELL_ROUND_FOCUSED_SHORT_CELL_HEIGHT
                                            : MENU_CELL_ROUND_UNFOCUSED_SHORT_CELL_HEIGHT;
#endif
}

static void list_action_select(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ml;
  (void)ctx;
  if (idx->row == 0) {
    int li = s_list_ix;
    window_stack_pop(true);
    s_win = NULL;
    s_menu = NULL;
    messaging_send(CMD_W_DELETE_LIST, li, -1, NULL);
  } else if (idx->row == 1) {
    if (s_saved_title) {
      persist_write_string(PERSIST_KEY_DEFAULT_LIST_TITLE, s_saved_title);
      ui_toast_show("Default list set");
    }
    window_stack_pop(true);
    s_win = NULL;
    s_menu = NULL;
  } else if (idx->row == 2) {
    if (s_saved_title) {
      bool is_synced = is_list_timeline_synced(s_saved_title);
      bool new_state = !is_synced;
      set_list_timeline_synced(s_saved_title, new_state);
      messaging_send(CMD_W_SET_TIMELINE_SYNC, s_list_ix, -1, new_state ? "1" : "0");
      ui_toast_show(new_state ? "Sync enabled" : "Sync disabled");
      menu_layer_reload_data(s_menu);
    }
  }
}

#ifdef PBL_TOUCH
static void list_action_touch_appear(Window *w) {
  (void)w;
  if (!s_menu || !s_win) {
    menu_layer_touch_on_window_disappear();
    return;
  }
  MenuLayerTouchHooks hooks = {.menu = s_menu,
                             .callback_context = NULL,
                             .get_num_rows = list_action_get_rows,
                             .get_cell_height = list_action_get_cell_height,
                             .select_click = list_action_select};
  menu_layer_touch_on_window_appear(&hooks);
}

static void list_action_touch_disappear(Window *w) {
  (void)w;
  menu_layer_touch_on_window_disappear();
}
#endif

static void list_action_load(Window *w) {
  (void)w;
  Layer *wl = window_get_root_layer(s_win);
  GRect wb = layer_get_bounds(wl);
#ifdef PBL_ROUND
  GRect mb = wb;
#else
  s_rect_title = text_layer_create(GRect(0, 0, wb.size.w, UI_CLOCK_BAR_HEIGHT));
  text_layer_set_text(s_rect_title, s_saved_title ? s_saved_title : "");
  text_layer_set_font(s_rect_title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(s_rect_title, theme_text());
  text_layer_set_background_color(s_rect_title, theme_bg());
  text_layer_set_text_alignment(s_rect_title, GTextAlignmentCenter);
  layer_add_child(wl, text_layer_get_layer(s_rect_title));
  GRect mb = GRect(0, UI_CLOCK_BAR_HEIGHT, wb.size.w, wb.size.h - UI_CLOCK_BAR_HEIGHT);
#endif

  s_menu = menu_layer_create(mb);
  menu_layer_set_click_config_onto_window(s_menu, s_win);
  menu_layer_set_center_focused(s_menu, PBL_IF_ROUND_ELSE(true, false));
  menu_layer_set_callbacks(
      s_menu, NULL,
      (MenuLayerCallbacks){
          .get_num_rows = list_action_get_rows,
          .draw_row = list_action_draw_row,
          .get_cell_height = list_action_get_cell_height,
          .select_click = list_action_select,
      });
  menu_layer_set_normal_colors(s_menu, theme_bg(), theme_text());
  menu_layer_set_highlight_colors(s_menu, theme_menu_highlight_bg(), theme_menu_highlight_text());
  menu_layer_configure_scroll_behavior(s_menu);
  layer_add_child(wl, menu_layer_get_layer(s_menu));
}

static void list_action_unload(Window *w) {
  (void)w;
#ifndef PBL_ROUND
  if (s_rect_title) {
    text_layer_destroy(s_rect_title);
    s_rect_title = NULL;
  }
#endif
  if (s_menu) {
    menu_layer_destroy(s_menu);
    s_menu = NULL;
  }
  clear_saved_strings();
  s_win = NULL;
  main_menu_cancel_select_suppress();
}

void list_action_menu_show(int list_index, const char *list_title) {
  if (s_win) {
    return;
  }
  s_list_ix = list_index;
  clear_saved_strings();
  s_saved_title = str_util_strdup(list_title ? list_title : "");
  if (!s_saved_title) {
    return;
  }
  s_win = window_create();
  window_set_window_handlers(
      s_win,
      (WindowHandlers){.load = list_action_load,
                       .unload = list_action_unload,
#ifdef PBL_TOUCH
                       .appear = list_action_touch_appear,
                       .disappear = list_action_touch_disappear,
#endif
      });
  window_set_background_color(s_win, theme_bg());
  window_stack_push(s_win, true);
}

void list_action_menu_apply_theme(void) {
  if (!s_win || !s_menu) {
    return;
  }
#ifndef PBL_ROUND
  if (s_rect_title) {
    text_layer_set_text_color(s_rect_title, theme_text());
    text_layer_set_background_color(s_rect_title, theme_bg());
  }
#endif
  window_set_background_color(s_win, theme_bg());
  menu_layer_set_normal_colors(s_menu, theme_bg(), theme_text());
  menu_layer_set_highlight_colors(s_menu, theme_menu_highlight_bg(), theme_menu_highlight_text());
  menu_layer_reload_data(s_menu);
}
