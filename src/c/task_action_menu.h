#pragma once

#include <pebble.h>

/** Long-press menu for an open task row (list index + task index among open tasks). */
void task_action_menu_show(int list_index, int task_index, const char *task_title,
                           const char *task_due_display_or_null, const char *task_due_iso_yyyy_mm_dd_or_null);

void task_action_menu_apply_theme(void);
