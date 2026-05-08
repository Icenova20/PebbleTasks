#pragma once

#include <pebble.h>
#include <stdbool.h>
#include <stdint.h>

typedef uint16_t (*MenuTouchGetNumRows)(MenuLayer *ml, uint16_t section, void *cb_ctx);
typedef int16_t (*MenuTouchGetCellHeight)(MenuLayer *ml, MenuIndex *idx, void *cb_ctx);
typedef void (*MenuTouchSelect)(MenuLayer *ml, MenuIndex *idx, void *cb_ctx);

typedef struct {
  MenuLayer *menu;
  void *callback_context;
  MenuTouchGetNumRows get_num_rows;
  MenuTouchGetCellHeight get_cell_height;
  MenuTouchSelect select_click;
} MenuLayerTouchHooks;

/**
 * Subscribe to TouchService while this window is the top-visible window (call from Handler.appear).
 * Pass NULL hooks (or hooks->menu == NULL via appear impl) clears via shutdown path.
 */
void menu_layer_touch_on_window_appear(const MenuLayerTouchHooks *hooks);

/** Unsubscribe from TouchService and reset gesture state. */
void menu_layer_touch_on_window_disappear(void);

/** Disable ScrollLayer paging so UP/DOWN move continuously instead of full-page jumps. */
void menu_layer_configure_scroll_behavior(MenuLayer *menu);
