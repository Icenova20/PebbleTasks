#pragma once

#include <pebble.h>
#include <stdint.h>

/** Single appearance: light (white background, black text). */
#define THEME_NUM_PRESETS 1

void theme_init(void);
void theme_set_from_phone(int32_t preset_id);
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

void theme_add_button_colors(bool highlighted, GColor *out_disk, GColor *out_plus);
GColor theme_checkbox_stroke(bool highlighted);
/** Monochrome row icons: dark line art on light background. */
bool theme_icons_use_light_variant(void);

void theme_apply_all(void);
void theme_handle_inbox(DictionaryIterator *it);
