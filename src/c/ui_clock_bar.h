#pragma once

#include <pebble.h>

/**
 * Replaces the status bar on rect: time + app name. Typography matches PebbleChecklist
 * secondary scale (GOTHIC_18_BOLD / GOTHIC_18); B&W no text AA to reduce mottled grain.
 * https://github.com/freakified/PebbleChecklist
 */
#define UI_CLOCK_BAR_HEIGHT 24

Layer *ui_clock_bar_create_for_window_size(GSize size);
void ui_clock_bar_unlink_and_destroy(Layer *clock_bar);
void ui_clock_bar_invalidate(Layer *clock_bar);
