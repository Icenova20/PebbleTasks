#pragma once

#include <pebble.h>
#include <stdbool.h>

void completed_menu_deinit(void);

void completed_menu_push(int list_index);
void completed_menu_reload_from_payload_if_visible(const char *payload, int list_index);

void completed_menu_window_load(Window *w);
void completed_menu_window_unload(Window *w);
void completed_menu_apply_theme(void);
