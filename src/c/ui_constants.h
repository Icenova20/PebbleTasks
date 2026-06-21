#pragma once

#include <pebble.h>

#define UI_TOAST_HEIGHT 36
#define UI_TOAST_FONT_KEY FONT_KEY_GOTHIC_18_BOLD

/** Matches Settings/basic menu typography for wrapped rows. */
#define UI_MENU_FONT_KEY FONT_KEY_GOTHIC_24_BOLD
#define UI_TASK_DUE_FONT_KEY FONT_KEY_GOTHIC_18_BOLD

#define UI_CELL_MARGIN 5
#define UI_TASK_DUE_GAP 2
#define UI_CELL_MIN_HEIGHT PBL_IF_ROUND_ELSE(49, 45)
#define UI_CELL_MAX 82
