#include "theme.h"

#include "completed_menu.h"
#include "main_menu.h"
#include "messaging.h"
#include "tasks_menu.h"
#include "ui_loading.h"
#include "ui_toast.h"

#include <message_keys.auto.h>
#include <pebble.h>
#include <stdint.h>

#define PERSIST_KEY_THEME 200

static int s_preset;
static GColor s_bg, s_text, s_hi_bg, s_hi_text, s_accent, s_status_fg, s_toast_bg, s_toast_text;

static void load_palette_for_preset(int p) {
  if (p < 0) {
    p = 0;
  }
  if (p >= THEME_NUM_PRESETS) {
    p = THEME_NUM_PRESETS - 1;
  }
  s_preset = p;
  /* One 64-color palette for all hardware; dithering on 1-bit. Dark only. */
  s_bg = GColorBlack;
  s_text = GColorWhite;
  s_hi_bg = GColorDukeBlue;
  s_hi_text = GColorWhite;
  s_accent = GColorVividCerulean;
  s_status_fg = GColorWhite;
  s_toast_bg = GColorDarkGray;
  s_toast_text = GColorWhite;
}

void theme_init(void) {
  int p = 0;
  if (persist_exists(PERSIST_KEY_THEME)) {
    int v = (int)persist_read_int(PERSIST_KEY_THEME);
    if (v >= 0 && v < THEME_NUM_PRESETS) {
      p = v;
    }
  }
  load_palette_for_preset(p);
}

static void persist_and_refresh(int p) {
  load_palette_for_preset(p);
  persist_write_int(PERSIST_KEY_THEME, s_preset);
}

GColor theme_bg(void) { return s_bg; }
GColor theme_text(void) { return s_text; }
GColor theme_highlight_bg(void) { return s_hi_bg; }
GColor theme_highlight_text(void) { return s_hi_text; }
GColor theme_accent(void) { return s_accent; }
GColor theme_status_fg(void) { return s_status_fg; }
GColor theme_toast_bg(void) { return s_toast_bg; }
GColor theme_toast_text(void) { return s_toast_text; }

GColor theme_menu_subtle_text(void) { return s_accent; }

bool theme_icons_use_light_variant(void) {
  return true;
}

void theme_add_button_colors(bool hi, GColor *out_disk, GColor *out_plus) {
  if (!out_disk || !out_plus) {
    return;
  }
  if (hi) {
    *out_disk = s_hi_text;
    *out_plus = s_accent;
  } else {
    *out_disk = s_accent;
    *out_plus = GColorWhite;
  }
}

GColor theme_checkbox_stroke(bool hi) { return hi ? s_hi_text : s_text; }

void theme_set_from_phone(int32_t preset_id) {
  if (preset_id < 0 || preset_id >= THEME_NUM_PRESETS) {
    return;
  }
  persist_and_refresh((int)preset_id);
  theme_apply_all();
}

void theme_apply_all(void) {
  main_menu_apply_theme();
  tasks_menu_apply_theme();
  completed_menu_apply_theme();
  ui_toast_apply_theme();
  ui_loading_invalidate();
}

void theme_handle_inbox(DictionaryIterator *it) {
  Tuple *t = dict_find(it, MESSAGE_KEY_themePreset);
  if (!t) {
    return;
  }
  int32_t v = messaging_tuple_read_s32(t);
  theme_set_from_phone(v);
}
