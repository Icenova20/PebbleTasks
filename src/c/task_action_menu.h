#pragma once

#include <pebble.h>

/** Long-press menu for an open task row (list index + task index among open tasks). */
void task_action_menu_show(int list_index, int task_index, const char *task_title,
                           const char *task_due_or_null);

void task_action_menu_apply_theme(void);
