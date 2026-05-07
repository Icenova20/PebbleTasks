#pragma once

#include <pebble.h>

/** Fixed light appearance (white background, black text, Google blue accents). */

void theme_init(void);
GColor theme_bg(void);
GColor theme_text(void);
GColor theme_highlight_bg(void);
GColor theme_highlight_text(void);
/** Background for MenuLayer / wrapped rows (Google-blue primary ~#4285F4). */
GColor theme_menu_highlight_bg(void);
/** Foreground for MenuLayer / wrapped menu rows on menu highlight bg (same as body text). */
GColor theme_menu_highlight_text(void);
GColor theme_accent(void);
/** Loading spinner arc on white (darker blue than menu highlight). */
GColor theme_loading_indicator(void);
GColor theme_status_fg(void);
GColor theme_toast_bg(void);
GColor theme_toast_text(void);

void theme_apply_all(void);
