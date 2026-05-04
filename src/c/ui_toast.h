#pragma once

#include <pebble.h>

void ui_toast_init(Layer *root_layer);
void ui_toast_deinit(void);
/** Remove toast from this window’s layer tree before the window is destroyed (keeps TextLayer alive). */
void ui_toast_detach_from_window(Window *window);
void ui_toast_show(const char *msg);
void ui_toast_apply_theme(void);
