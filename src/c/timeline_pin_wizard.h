#pragma once

#include <pebble.h>

/**
 * Pick local calendar date + time for a timeline pin; sends CMD_W_PIN_TASK with text
 * `YYYY-MM-DDTHH:MM:SSZ` (UTC). Optional due_iso is `YYYY-MM-DD` from the task for defaults.
 */
void timeline_pin_wizard_push(int list_index, int task_index, const char *due_iso_yyyy_mm_dd_or_null);

void timeline_pin_wizard_apply_theme(void);
