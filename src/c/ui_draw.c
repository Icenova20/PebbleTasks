#include "ui_draw.h"
#include "ui_assets.h"
#include "ui_constants.h"

/* Playback-style: on B&W, disable text AA to reduce mottled “grain” on 1-bit; color keeps smooth AA. */
static void menu_text_antialias(GContext *ctx) { graphics_context_set_antialiased(ctx, PBL_IF_BW_ELSE(false, true)); }

int ui_draw_text_cell_width(const Layer *window_layer) {
  GRect wb = layer_get_bounds(window_layer);
#ifdef PBL_ROUND
  return wb.size.w - TASK_CHECKBOX_SIZE * 4;
#else
  return wb.size.w - UI_CELL_MARGIN * 2;
#endif
}

int ui_draw_task_row_text_layout_width(const Layer *window_layer) {
  GRect wb = layer_get_bounds(window_layer);
  int sw = wb.size.w;
#ifdef PBL_ROUND
  return sw - TASK_CHECKBOX_SIZE * 4;
#else
  /* Checkbox on the left + margins */
  return sw - UI_CELL_MARGIN * 3 - TASK_CHECKBOX_SIZE;
#endif
}

void ui_draw_menu_title_row(GContext *ctx, const Layer *cell_layer, const char *title, bool accent_subtle,
                            GTextAttributes *round_flow_attr) {
  GRect bounds = layer_get_bounds(cell_layer);
  bool hi = menu_cell_layer_is_highlighted(cell_layer);
  if (accent_subtle && !hi) {
    graphics_context_set_text_color(ctx, theme_menu_subtle_text());
  } else {
    graphics_context_set_text_color(ctx, hi ? theme_highlight_text() : theme_text());
  }
  menu_text_antialias(ctx);
#ifdef PBL_ROUND
  {
    int th = bounds.size.h - UI_MENU_TEXT_Y;
    if (th < 1) {
      th = bounds.size.h;
    }
    GRect tb = GRect(UI_CELL_MARGIN, UI_MENU_TEXT_Y, bounds.size.w - TASK_CHECKBOX_SIZE * 4, th);
    graphics_draw_text(ctx, title, fonts_get_system_font(UI_MENU_FONT_KEY), tb,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, round_flow_attr);
  }
#else
  {
    int th = bounds.size.h - UI_MENU_TEXT_Y;
    if (th < 1) {
      th = bounds.size.h;
    }
    GRect tb = GRect(UI_CELL_MARGIN, UI_MENU_TEXT_Y, bounds.size.w - UI_CELL_MARGIN * 2, th);
    graphics_draw_text(ctx, title, fonts_get_system_font(UI_MENU_FONT_KEY), tb,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
#endif
}

void ui_draw_menu_leading_icon_row(GContext *ctx, const Layer *cell_layer, const GBitmap *icon, const char *text,
                                    bool accent_subtle, GTextAttributes *round_flow_attr) {
  GRect bounds = layer_get_bounds(cell_layer);
  bool hi = menu_cell_layer_is_highlighted(cell_layer);
  if (accent_subtle && !hi) {
    graphics_context_set_text_color(ctx, theme_menu_subtle_text());
  } else {
    graphics_context_set_text_color(ctx, hi ? theme_highlight_text() : theme_text());
  }
  menu_text_antialias(ctx);

  int ix = UI_CELL_MARGIN;
  int iw = 0, ih = 0;
  if (icon) {
    GRect ibr = gbitmap_get_bounds(icon);
    iw = ibr.size.w;
    ih = ibr.size.h;
    int iy = (bounds.size.h - ih) / 2;
    if (iy < 0) {
      iy = 0;
    }
    graphics_draw_bitmap_in_rect(ctx, icon, GRect(ix, iy, iw, ih));
  }
  int text_x = icon ? (ix + iw + 3) : UI_CELL_MARGIN;
  int th = bounds.size.h - UI_MENU_TEXT_Y;
  if (th < 1) {
    th = bounds.size.h;
  }
  GRect tb = GRect(text_x, UI_MENU_TEXT_Y, bounds.size.w - text_x - UI_CELL_MARGIN, th);
#ifdef PBL_ROUND
  graphics_draw_text(ctx, text, fonts_get_system_font(UI_MENU_FONT_KEY), tb, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, round_flow_attr);
#else
  graphics_draw_text(ctx, text, fonts_get_system_font(UI_MENU_FONT_KEY), tb, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
#endif
}

#ifndef PBL_ROUND
static void draw_add_plus_at(GContext *ctx, GPoint c, bool highlighted) {
  GColor disk;
  GColor plus;
  theme_add_button_colors(highlighted, &disk, &plus);
  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_fill_color(ctx, disk);
  graphics_fill_circle(ctx, c, ADD_ROW_BTN_R);
  graphics_context_set_fill_color(ctx, plus);
  int arm = ADD_ROW_BTN_ARM;
  int bar = ADD_ROW_BTN_BAR;
  int w = arm * 2;
  graphics_fill_rect(ctx, GRect(c.x - arm, c.y - bar / 2, w, bar), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(c.x - bar / 2, c.y - arm, bar, w), 0, GCornerNone);
  graphics_context_set_antialiased(ctx, PBL_IF_BW_ELSE(true, true));
}
#endif

void ui_draw_add_labeled_row(GContext *ctx, const Layer *cell_layer, bool is_task_list,
                            GTextAttributes *round_attr) {
  GRect b = layer_get_bounds(cell_layer);
  bool hi = menu_cell_layer_is_highlighted(cell_layer);
  const GBitmap *ic = is_task_list ? ui_assets_add_list() : ui_assets_add_task();
  const char *label = is_task_list ? "Add list" : "Add task";
  menu_text_antialias(ctx);
  GFont f = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  graphics_context_set_text_color(ctx, hi ? theme_highlight_text() : theme_text());

  int ix = UI_CELL_MARGIN;
  int iw = 0, ih = 0;
  if (ic) {
    GRect ibr = gbitmap_get_bounds(ic);
    iw = ibr.size.w;
    ih = ibr.size.h;
    int iy = (b.size.h - ih) / 2;
    if (iy < 0) {
      iy = 0;
    }
    graphics_draw_bitmap_in_rect(ctx, ic, GRect(ix, iy, iw, ih));
  }
  int text_x = ic ? (ix + iw + 3) : UI_CELL_MARGIN;

#ifdef PBL_ROUND
  {
    int th = b.size.h - UI_MENU_TEXT_Y;
    if (th < 1) {
      th = b.size.h;
    }
    GRect tr = GRect(text_x, UI_MENU_TEXT_Y, b.size.w - text_x - UI_CELL_MARGIN, th);
    graphics_draw_text(ctx, label, f, tr, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, round_attr);
  }
#else
  {
    GPoint c = GPoint(b.size.w - UI_CELL_MARGIN - ADD_ROW_BTN_R, b.size.h / 2);
    draw_add_plus_at(ctx, c, hi);
    int right_pad = (ADD_ROW_BTN_R * 2 + 6);
    int left_w = b.size.w - text_x - UI_CELL_MARGIN - right_pad;
    if (left_w < 20) {
      left_w = 20;
    }
    int th = b.size.h - UI_MENU_TEXT_Y;
    if (th < 1) {
      th = b.size.h;
    }
    GRect tr = GRect(text_x, UI_MENU_TEXT_Y, left_w, th);
    graphics_draw_text(ctx, label, f, tr, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
#endif
}

void ui_draw_tasks_checkbox_frame(GContext *ctx, const Layer *cell_layer) {
  GRect bounds = layer_get_bounds(cell_layer);
  bool hi = menu_cell_layer_is_highlighted(cell_layer);
  graphics_context_set_stroke_color(ctx, theme_checkbox_stroke(hi));
  bool show_checkbox = true;
#ifdef PBL_ROUND
  show_checkbox = menu_cell_layer_is_highlighted(cell_layer);
#endif
  if (show_checkbox) {
#ifdef PBL_ROUND
    GRect r = GRect(bounds.size.w - (2 * TASK_CHECKBOX_SIZE),
                      (bounds.size.h / 2) - (TASK_CHECKBOX_SIZE / 2), TASK_CHECKBOX_SIZE,
                      TASK_CHECKBOX_SIZE);
#else
    GRect r = GRect(UI_CELL_MARGIN, (bounds.size.h / 2) - (TASK_CHECKBOX_SIZE / 2),
                      TASK_CHECKBOX_SIZE, TASK_CHECKBOX_SIZE);
#endif
    graphics_draw_rect(ctx, r);
  }
}

void ui_draw_tasks_open_cell(GContext *ctx, const Layer *cell_layer, const char *title,
                             GTextAttributes *round_flow) {
  GRect bounds = layer_get_bounds(cell_layer);

  bool hi = menu_cell_layer_is_highlighted(cell_layer);
  graphics_context_set_text_color(ctx, hi ? theme_highlight_text() : theme_text());
  menu_text_antialias(ctx);

#ifdef PBL_ROUND
  {
    int th = bounds.size.h - UI_MENU_TEXT_Y;
    if (th < 1) {
      th = bounds.size.h;
    }
    GRect text_bounds = GRect(UI_CELL_MARGIN, UI_MENU_TEXT_Y, bounds.size.w - TASK_CHECKBOX_SIZE * 4, th);
    graphics_draw_text(ctx, title, fonts_get_system_font(UI_MENU_FONT_KEY), text_bounds,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, round_flow);
  }
#else
  {
    int left_text = UI_CELL_MARGIN + TASK_CHECKBOX_SIZE + UI_CELL_MARGIN;
    int th = bounds.size.h - UI_MENU_TEXT_Y;
    if (th < 1) {
      th = bounds.size.h;
    }
    GRect text_bounds =
        GRect(left_text, UI_MENU_TEXT_Y, bounds.size.w - left_text - UI_CELL_MARGIN, th);
    graphics_draw_text(ctx, title, fonts_get_system_font(UI_MENU_FONT_KEY), text_bounds,
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
#endif

  ui_draw_tasks_checkbox_frame(ctx, cell_layer);
}
