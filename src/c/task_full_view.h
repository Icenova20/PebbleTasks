#pragma once

#include <pebble.h>

/** Read-only scrollable task title + optional due; owns a copy of text until closed. */
void task_full_view_push(const char *title, const char *due_or_null);

void task_full_view_apply_theme(void);
