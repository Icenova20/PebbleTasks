#pragma once

#include <pebble.h>

void ui_toast_init(Layer *root_layer);
void ui_toast_deinit(void);
void ui_toast_show(const char *msg);
void ui_toast_apply_theme(void);
