#include "ui_menu_wrap_cell.h"

#include "theme.h"
#include "ui_constants.h"

#ifdef PBL_ROUND
static GTextAttributes *s_wrap_attrs;

static GTextAttributes *wrap_attrs(void) {
  if (!s_wrap_attrs) {
    s_wrap_attrs = graphics_text_attributes_create();
    graphics_text_attributes_enable_screen_text_flow(s_wrap_attrs, UI_CELL_MARGIN * 2);
  }
  return s_wrap_attrs;
}
#else
#define wrap_attrs() ((GTextAttributes *)NULL)
#endif

int ui_menu_wrap_cell_text_width(const Layer *window_layer) {
  GRect wb = layer_get_bounds(window_layer);
  int tw = wb.size.w - UI_CELL_MARGIN * 2;
  if (tw < 8) {
    tw = 8;
  }
  return tw;
}

static int measured_body_height_px(int text_w, const char *title, const char *subtitle, GTextAttributes *attrs,
                                   GTextAlignment align) {
  GFont tf = fonts_get_system_font(UI_MENU_FONT_KEY);
  const char *t = (title && title[0]) ? title : " ";
  GSize tsz = graphics_text_layout_get_content_size_with_attributes(
      t, tf, GRect(0, 0, text_w, 5000), GTextOverflowModeWordWrap, align, attrs);
  int h = UI_CELL_MARGIN + (int)tsz.h;
  if (subtitle && subtitle[0]) {
    GFont sf = fonts_get_system_font(UI_TASK_DUE_FONT_KEY);
    GSize ssz = graphics_text_layout_get_content_size_with_attributes(
        subtitle, sf, GRect(0, 0, text_w, 5000), GTextOverflowModeWordWrap, align, attrs);
    h += UI_TASK_DUE_GAP + (int)ssz.h;
  }
  h += UI_CELL_MARGIN;
  return h;
}

int16_t ui_menu_wrap_cell_measure_height(const Layer *window_layer, const char *title, const char *subtitle) {
  int w = ui_menu_wrap_cell_text_width(window_layer);
  GTextAttributes *attrs = wrap_attrs();
  GTextAlignment align = PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft);

  int h = measured_body_height_px(w, title, subtitle, attrs, align);
  if (h > UI_CELL_MAX) {
    h = UI_CELL_MAX;
  }
  return (int16_t)h;
}

void ui_menu_wrap_cell_draw(GContext *ctx, const Layer *cell_layer, MenuLayer *menu_layer,
                            MenuIndex *cell_index, const char *title, const char *subtitle,
                            const GBitmap *icon) {
  if (!title || !title[0]) {
    return;
  }

  GRect b = layer_get_bounds(cell_layer);
  bool sel = menu_layer && cell_index && menu_layer_is_index_selected(menu_layer, cell_index);
  graphics_context_set_fill_color(ctx, sel ? theme_menu_highlight_bg() : theme_bg());
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  int text_left = UI_CELL_MARGIN;
  int text_w = b.size.w - 2 * UI_CELL_MARGIN;

  if (icon) {
    GRect ib = gbitmap_get_bounds(icon);
    int iy = (int)b.size.h / 2 - ib.size.h / 2;
    if (iy < 0) {
      iy = 0;
    }
    graphics_draw_bitmap_in_rect(ctx, icon,
                                 GRect(UI_CELL_MARGIN, iy, ib.size.w, ib.size.h));
    text_left = UI_CELL_MARGIN + ib.size.w + 4;
    text_w = b.size.w - text_left - UI_CELL_MARGIN;
    if (text_w < 8) {
      text_w = 8;
    }
  }

  GTextAlignment align =
      icon ? GTextAlignmentLeft : PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft);
  GTextAttributes *attrs = wrap_attrs();

  GFont tf = fonts_get_system_font(UI_MENU_FONT_KEY);
  GSize tsz = graphics_text_layout_get_content_size_with_attributes(
      title, tf, GRect(0, 0, text_w, 5000), GTextOverflowModeWordWrap, align, attrs);

  GSize ssz = {0, 0};
  if (subtitle && subtitle[0]) {
    GFont sf = fonts_get_system_font(UI_TASK_DUE_FONT_KEY);
    ssz = graphics_text_layout_get_content_size_with_attributes(
        subtitle, sf, GRect(0, 0, text_w, 5000), GTextOverflowModeWordWrap, align, attrs);
  }

  int inner_h = (int)b.size.h - UI_CELL_MARGIN * 2;
  if (inner_h < 0) {
    inner_h = 0;
  }
  int block_h = (int)tsz.h;
  if (subtitle && subtitle[0]) {
    block_h += UI_TASK_DUE_GAP + (int)ssz.h;
  }

  int y0 = UI_CELL_MARGIN;
  int title_draw_h = (int)tsz.h;
  int subt_draw_h = (subtitle && subtitle[0]) ? (int)ssz.h : 0;
  if (block_h <= inner_h) {
    y0 = UI_CELL_MARGIN + (inner_h - block_h) / 2;
  } else {
    title_draw_h = (int)tsz.h;
    if (title_draw_h > inner_h) {
      title_draw_h = inner_h;
    }
    int rem = inner_h - title_draw_h;
    if (subtitle && subtitle[0]) {
      rem -= UI_TASK_DUE_GAP;
      if (rem < 0) {
        rem = 0;
      }
      subt_draw_h = (int)ssz.h;
      if (subt_draw_h > rem) {
        subt_draw_h = rem;
      }
    }
  }

  GRect tr = GRect(text_left, y0, text_w, title_draw_h);
  graphics_context_set_text_color(ctx, sel ? theme_menu_highlight_text() : theme_text());
  graphics_draw_text(ctx, title, tf, tr, GTextOverflowModeWordWrap, align, attrs);

  if (subtitle && subtitle[0] && subt_draw_h > 0) {
    int y = y0 + title_draw_h + UI_TASK_DUE_GAP;
    GFont sf = fonts_get_system_font(UI_TASK_DUE_FONT_KEY);
    GRect sr = GRect(text_left, y, text_w, subt_draw_h);
    graphics_context_set_text_color(ctx, sel ? theme_menu_highlight_text() : theme_menu_subtle_text());
    graphics_draw_text(ctx, subtitle, sf, sr, GTextOverflowModeWordWrap, align, attrs);
  }
}
