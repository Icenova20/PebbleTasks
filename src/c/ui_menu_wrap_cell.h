#pragma once

#include <pebble.h>

/** Height for one wrapped title block plus optional subtitle; clamped to [floor, UI_CELL_MAX]. */
int16_t ui_menu_wrap_cell_measure_height(MenuLayer *menu_layer, const char *title, const char *subtitle);

/**
 * Word-wrapped menu body cell matching MenuLayer normal/highlight colors.
 * Pass icon NULL for full-width text (tasks/main/completed title rows).
 */
void ui_menu_wrap_cell_draw(GContext *ctx, const Layer *cell_layer, MenuLayer *menu_layer,
                            MenuIndex *cell_index, const char *title, const char *subtitle,
                            const GBitmap *icon);
