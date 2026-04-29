#pragma once

#include <pebble.h>

/**
 * Replaces Pebble's StatusBarLayer for rectangular models: custom time + brand
 * (non-bold, smaller GOTHIC_14) + B&W no-antialias to reduce mottled “grain.”
 */
#define UI_CLOCK_BAR_HEIGHT 20

Layer *ui_clock_bar_create_for_window_size(GSize size);
void ui_clock_bar_unlink_and_destroy(Layer *clock_bar);
void ui_clock_bar_invalidate(Layer *clock_bar);
