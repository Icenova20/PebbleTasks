#include "task_action_menu.h"

#include "due_wizard.h"
#include "messaging.h"
#include "protocol.h"
#include "theme.h"
#include "ui_constants.h"
#include "ui_clock_bar.h"

#include <pebble.h>

static Window *s_win;
static MenuLayer *s_menu;
#ifndef PBL_ROUND
static TextLayer *s_rect_title;
#endif
static int s_list_ix;
static int s_task_ix;

static uint16_t action_get_rows(MenuLayer *ml, uint16_t section, void *ctx) {
  (void)ml;
  (void)section;
  (void)ctx;
  return 4;
}

static void action_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *idx, void *cb_ctx) {
  (void)cb_ctx;
  const char *titles[] = {"Mark complete", "Due date…", "Clear due", "Delete task"};
  int i = idx->row;
  if (i < 0 || i > 3) {
    return;
  }
  menu_cell_basic_draw(ctx, cell_layer, titles[i], NULL, NULL);
}

static int16_t action_cell_height(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ml;
  (void)idx;
  (void)ctx;
  return UI_CELL_MIN;
}

static void action_select(MenuLayer *ml, MenuIndex *idx, void *ctx) {
  (void)ml;
  (void)ctx;
  int row = idx->row;
  int li = s_list_ix;
  int ti = s_task_ix;
  window_stack_pop(true);
  s_win = NULL;
  s_menu = NULL;
  if (row == 0) {
    messaging_send(CMD_W_COMPLETE, li, ti, NULL);
    return;
  }
  if (row == 1) {
    due_wizard_push(li, ti);
    return;
  }
  if (row == 2) {
    messaging_send(CMD_W_CLEAR_TASK_DUE, li, ti, NULL);
    return;
  }
  messaging_send(CMD_W_DELETE_TASK, li, ti, NULL);
}

static void action_load(Window *w) {
  (void)w;
  Layer *wl = window_get_root_layer(s_win);
  GRect wb = layer_get_bounds(wl);
#ifdef PBL_ROUND
  GRect mb = wb;
#else
  s_rect_title = text_layer_create(GRect(0, 0, wb.size.w, UI_CLOCK_BAR_HEIGHT));
  text_layer_set_text(s_rect_title, "Task");
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
          .get_num_rows = action_get_rows,
          .draw_row = action_draw_row,
          .get_cell_height = action_cell_height,
          .select_click = action_select,
      });
  menu_layer_set_normal_colors(s_menu, theme_bg(), theme_text());
  menu_layer_set_highlight_colors(s_menu, theme_highlight_bg(), theme_highlight_text());
  layer_add_child(wl, menu_layer_get_layer(s_menu));
}

static void action_unload(Window *w) {
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
  s_win = NULL;
}

void task_action_menu_show(int list_index, int task_index) {
  s_list_ix = list_index;
  s_task_ix = task_index;
  if (s_win) {
    return;
  }
  s_win = window_create();
  window_set_window_handlers(s_win, (WindowHandlers){.load = action_load, .unload = action_unload});
  window_set_background_color(s_win, theme_bg());
  window_stack_push(s_win, true);
}

void task_action_menu_apply_theme(void) {
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
  menu_layer_set_highlight_colors(s_menu, theme_highlight_bg(), theme_highlight_text());
  menu_layer_reload_data(s_menu);
}
