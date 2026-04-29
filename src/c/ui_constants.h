#pragma once

#include "theme.h"
#include <pebble.h>

#define UI_BG_COLOR (theme_bg())
#define UI_CELL_MIN PBL_IF_ROUND_ELSE(44, 42)
#define UI_CELL_MAX 80
#define UI_CELL_MARGIN 3
#define TASK_CHECKBOX_SIZE 12
/** List / task lines: regular (non-bold) for clearer 8-bit / Emery. */
#define UI_MENU_FONT_KEY FONT_KEY_GOTHIC_24
/* Subtle vertical nudge (Playback LIST_TITLE_Y) so one-line text sits in the cell optical center. */
#define UI_MENU_TEXT_Y PBL_IF_ROUND_ELSE(0, -1)
#define UI_TOAST_HEIGHT 30
#define UI_TOAST_FONT_KEY FONT_KEY_GOTHIC_14

/* Smaller + glyph on the right; label on the left (rect). */
#define ADD_ROW_BTN_R 9
#define ADD_ROW_BTN_ARM 5
#define ADD_ROW_BTN_BAR 2
#define UI_ADD_ROW_CELL_H PBL_IF_ROUND_ELSE(40, 36)
