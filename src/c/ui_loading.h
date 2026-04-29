#pragma once

#include <pebble.h>

/** Full-screen overlay: rotating arc (custom Animation + draw_arc) + "Loading" label. */
void ui_loading_start(Layer *window_root_layer);
void ui_loading_stop(void);
void ui_loading_invalidate(void);
