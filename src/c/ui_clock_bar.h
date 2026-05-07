#pragma once

#include <pebble.h>

/**
 * Replaces the status bar on rect: time (left) + date (right). Gothic 18 bold.
 */
#define UI_CLOCK_BAR_HEIGHT 24

Layer *ui_clock_bar_create_for_window_size(GSize size);
void ui_clock_bar_unlink_and_destroy(Layer *clock_bar);
void ui_clock_bar_invalidate(Layer *clock_bar);
