#pragma once

#include <pebble.h>
#include <stdbool.h>

void main_menu_init(void);
void main_menu_deinit(void);
Window *main_menu_get_window(void);

void main_menu_reload_from_payload(const char *payload);
bool main_menu_is_adding_list(void);
void main_menu_set_adding_list(bool add_list);
void main_menu_apply_theme(void);

void main_menu_window_load(Window *w);
void main_menu_window_unload(Window *w);
