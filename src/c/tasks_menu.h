#pragma once

#include <pebble.h>
#include <stdbool.h>

void tasks_menu_deinit(void);

void tasks_menu_push(int list_index);
void tasks_menu_reload_from_payload_if_visible(const char *payload, int list_index, bool has_completed);
int tasks_menu_current_list_index(void);

void tasks_menu_window_load(Window *w);
void tasks_menu_window_unload(Window *w);
void tasks_menu_apply_theme(void);
