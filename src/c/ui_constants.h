#pragma once

#include "theme.h"
#include <pebble.h>

#define UI_BG_COLOR (theme_bg())
#define UI_CELL_MIN PBL_IF_ROUND_ELSE(44, 42)
#define UI_CELL_MAX 80
/* PebbleChecklist: CHECKLIST_CELL_MARGIN 5 */
#define UI_CELL_MARGIN 5
#define TASK_CHECKBOX_SIZE 12
/** List lines — PebbleChecklist: FONT_KEY_GOTHIC_24_BOLD (checklist_window). */
#define UI_MENU_FONT_KEY FONT_KEY_GOTHIC_24_BOLD
#define UI_TASK_DUE_FONT_KEY FONT_KEY_GOTHIC_18
#define UI_TASK_DUE_GAP 2
/* Subtle vertical nudge (Playback LIST_TITLE_Y) so one-line text sits in the cell optical center. */
#define UI_MENU_TEXT_Y PBL_IF_ROUND_ELSE(0, -1)
#define UI_TOAST_HEIGHT 36
/* PebbleChecklist: GOTHIC_18_BOLD for dialogs / empty state (dialog_message_window, checklist_window). */
#define UI_TOAST_FONT_KEY FONT_KEY_GOTHIC_18_BOLD

/* Smaller + glyph on the right; label on the left (rect). */
#define ADD_ROW_BTN_R 9
#define ADD_ROW_BTN_ARM 5
#define ADD_ROW_BTN_BAR 2
#define UI_ADD_ROW_CELL_H PBL_IF_ROUND_ELSE(40, 36)
