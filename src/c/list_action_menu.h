#pragma once

#include <pebble.h>

/** Long-press menu for a task list row on the main menu (list index among configured lists). */
void list_action_menu_show(int list_index, const char *list_title);

void list_action_menu_apply_theme(void);
