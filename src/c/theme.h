#pragma once

#include <pebble.h>
#include <stdint.h>

/** Number of named palette presets; must match settings UI and JS. */
#define THEME_NUM_PRESETS 4

void theme_init(void);
void theme_set_from_phone(int32_t preset_id);
GColor theme_bg(void);
GColor theme_text(void);
GColor theme_highlight_bg(void);
GColor theme_highlight_text(void);
GColor theme_accent(void);
GColor theme_status_fg(void);
GColor theme_toast_bg(void);
GColor theme_toast_text(void);

void theme_add_button_colors(bool highlighted, GColor *out_disk, GColor *out_plus);
GColor theme_checkbox_stroke(bool highlighted);
GColor theme_menu_subtle_text(void);
/** Monochrome row icons: use light line art on dark (preset Dark). */
bool theme_icons_use_light_variant(void);

void theme_apply_all(void);
void theme_handle_inbox(DictionaryIterator *it);
