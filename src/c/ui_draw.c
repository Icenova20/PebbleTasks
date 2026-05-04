#include "ui_draw.h"
#include "theme.h"
#include "ui_assets.h"
#include "ui_constants.h"

/* Playback-style: on B&W, disable text AA to reduce mottled “grain” on 1-bit; color keeps smooth AA. */
static void menu_text_antialias(GContext *ctx) { graphics_context_set_antialiased(ctx, PBL_IF_BW_ELSE(false, true)); }

static int list_row_y_nudged(int y) {
  y -= UI_MENU_TEXT_NUDGE_UP;
  return y < 0 ? 0 : y;
}

/** Thin 1px outline around the menu cell (local bounds). */
static void draw_list_cell_border(GContext *ctx, GRect bounds) {
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_stroke_color(ctx, theme_accent());
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, GRect(0, 0, bounds.size.w, bounds.size.h));
}

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
  draw_list_cell_border(ctx, bounds);
  bool hi = menu_cell_layer_is_highlighted(cell_layer);
  if (accent_subtle && !hi) {
    graphics_context_set_text_color(ctx, theme_menu_subtle_text());
  } else {
    graphics_context_set_text_color(ctx, hi ? theme_highlight_text() : theme_text());
  }
  menu_text_antialias(ctx);
  GFont menu_font = fonts_get_system_font(UI_MENU_FONT_KEY);
#ifdef PBL_ROUND
  {
    int avail_w = bounds.size.w - TASK_CHECKBOX_SIZE * 4;
    GSize tsz = graphics_text_layout_get_content_size_with_attributes(
        title, menu_font, GRect(0, 0, avail_w, 500), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentCenter, round_flow_attr);
    int ch = tsz.h;
    if (ch < 1) {
      ch = 1;
    }
    int y = list_row_y_nudged((bounds.size.h - ch) / 2);
    GRect tb = GRect(UI_CELL_MARGIN, y, avail_w, ch);
    graphics_draw_text(ctx, title, menu_font, tb, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       round_flow_attr);
  }
#else
  {
    int tw = bounds.size.w - UI_CELL_MARGIN * 2;
    GSize tsz = graphics_text_layout_get_content_size_with_attributes(
        title, menu_font, GRect(0, 0, tw, 500), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    int ch = tsz.h;
    if (ch < 1) {
      ch = 1;
    }
    int y = list_row_y_nudged((bounds.size.h - ch) / 2);
    GRect tb = GRect(UI_CELL_MARGIN, y, tw, ch);
    graphics_draw_text(ctx, title, menu_font, tb, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
#endif
}

void ui_draw_menu_leading_icon_row(GContext *ctx, const Layer *cell_layer, const GBitmap *icon, const char *text,
                                    bool accent_subtle, GTextAttributes *round_flow_attr) {
  GRect bounds = layer_get_bounds(cell_layer);
  draw_list_cell_border(ctx, bounds);
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
    int iy = list_row_y_nudged((bounds.size.h - ih) / 2);
    graphics_draw_bitmap_in_rect(ctx, icon, GRect(ix, iy, iw, ih));
  }
  int text_x = icon ? (ix + iw + 3) : UI_CELL_MARGIN;
  int tw = bounds.size.w - text_x - UI_CELL_MARGIN;
  GFont menu_font = fonts_get_system_font(UI_MENU_FONT_KEY);
  GSize tsz = graphics_text_layout_get_content_size_with_attributes(
      text, menu_font, GRect(0, 0, tw, 500), GTextOverflowModeTrailingEllipsis,
#ifdef PBL_ROUND
      GTextAlignmentLeft, round_flow_attr);
#else
      GTextAlignmentLeft, NULL);
#endif
  int ch = tsz.h;
  if (ch < 1) {
    ch = 1;
  }
  int y = list_row_y_nudged((bounds.size.h - ch) / 2);
  GRect tb = GRect(text_x, y, tw, ch);
#ifdef PBL_ROUND
  graphics_draw_text(ctx, text, menu_font, tb, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                     round_flow_attr);
#else
  graphics_draw_text(ctx, text, menu_font, tb, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
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
  draw_list_cell_border(ctx, b);
  bool hi = menu_cell_layer_is_highlighted(cell_layer);
  const GBitmap *ic = is_task_list ? ui_assets_add_list() : ui_assets_add_task();
  const char *label = is_task_list ? "Add list" : "Add task";
  menu_text_antialias(ctx);
  GFont f = fonts_get_system_font(UI_MENU_FONT_KEY);
  graphics_context_set_text_color(ctx, hi ? theme_highlight_text() : theme_text());

  int ix = UI_CELL_MARGIN;
  int iw = 0, ih = 0;
  if (ic) {
    GRect ibr = gbitmap_get_bounds(ic);
    iw = ibr.size.w;
    ih = ibr.size.h;
    int iy = list_row_y_nudged((b.size.h - ih) / 2);
    graphics_draw_bitmap_in_rect(ctx, ic, GRect(ix, iy, iw, ih));
  }
  int text_x = ic ? (ix + iw + 3) : UI_CELL_MARGIN;

#ifdef PBL_ROUND
  {
    int lw = b.size.w - text_x - UI_CELL_MARGIN;
    GSize tsz = graphics_text_layout_get_content_size_with_attributes(
        label, f, GRect(0, 0, lw, 500), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, round_attr);
    int ch = tsz.h;
    if (ch < 1) {
      ch = 1;
    }
    int y = list_row_y_nudged((b.size.h - ch) / 2);
    GRect tr = GRect(text_x, y, lw, ch);
    graphics_draw_text(ctx, label, f, tr, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, round_attr);
  }
#else
  {
    int cty = (int)(b.size.h / 2) - UI_MENU_TEXT_NUDGE_UP;
    if (cty < ADD_ROW_BTN_R + 1) {
      cty = ADD_ROW_BTN_R + 1;
    }
    GPoint c = GPoint(b.size.w - UI_CELL_MARGIN - ADD_ROW_BTN_R, cty);
    draw_add_plus_at(ctx, c, hi);
    int right_pad = (ADD_ROW_BTN_R * 2 + 6);
    int left_w = b.size.w - text_x - UI_CELL_MARGIN - right_pad;
    if (left_w < 20) {
      left_w = 20;
    }
    GSize tsz = graphics_text_layout_get_content_size_with_attributes(
        label, f, GRect(0, 0, left_w, 500), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    int ch = tsz.h;
    if (ch < 1) {
      ch = 1;
    }
    int y = list_row_y_nudged((b.size.h - ch) / 2);
    GRect tr = GRect(text_x, y, left_w, ch);
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
    int cy = list_row_y_nudged((bounds.size.h / 2) - (TASK_CHECKBOX_SIZE / 2));
#ifdef PBL_ROUND
    GRect r = GRect(bounds.size.w - (2 * TASK_CHECKBOX_SIZE), cy, TASK_CHECKBOX_SIZE, TASK_CHECKBOX_SIZE);
#else
    GRect r = GRect(UI_CELL_MARGIN, cy, TASK_CHECKBOX_SIZE, TASK_CHECKBOX_SIZE);
#endif
    graphics_draw_rect(ctx, r);
  }
}

void ui_draw_tasks_open_cell(GContext *ctx, const Layer *cell_layer, const char *title, const char *due,
                             GTextAttributes *round_flow) {
  GRect bounds = layer_get_bounds(cell_layer);
  draw_list_cell_border(ctx, bounds);

  bool hi = menu_cell_layer_is_highlighted(cell_layer);
  GFont title_font = fonts_get_system_font(UI_MENU_FONT_KEY);
  GFont due_font = fonts_get_system_font(UI_TASK_DUE_FONT_KEY);
  graphics_context_set_text_color(ctx, hi ? theme_highlight_text() : theme_text());
  menu_text_antialias(ctx);

#ifdef PBL_ROUND
  {
    int avail_w = bounds.size.w - TASK_CHECKBOX_SIZE * 4;
    GSize tsz = graphics_text_layout_get_content_size_with_attributes(
        title, title_font, GRect(0, 0, avail_w, 500), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft, round_flow);
    int title_h = tsz.h;
    if (title_h < 1) {
      title_h = 1;
    }
    int due_h = 0;
    if (due && due[0]) {
      GSize dsz = graphics_text_layout_get_content_size_with_attributes(
          due, due_font, GRect(0, 0, avail_w, 500), GTextOverflowModeTrailingEllipsis,
          GTextAlignmentLeft, round_flow);
      due_h = dsz.h;
      if (due_h < 1) {
        due_h = 1;
      }
    }
    int total = title_h + (due && due[0] ? UI_TASK_DUE_GAP + due_h : 0);
    int y0 = list_row_y_nudged((bounds.size.h - total) / 2);
    GRect title_bounds = GRect(UI_CELL_MARGIN, y0, avail_w, title_h);
    graphics_draw_text(ctx, title, title_font, title_bounds, GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, round_flow);
    if (due && due[0]) {
      graphics_context_set_text_color(ctx, hi ? theme_highlight_text() : theme_menu_subtle_text());
      GRect due_bounds =
          GRect(UI_CELL_MARGIN, y0 + title_h + UI_TASK_DUE_GAP, avail_w, due_h);
      graphics_draw_text(ctx, due, due_font, due_bounds, GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentLeft, round_flow);
      graphics_context_set_text_color(ctx, hi ? theme_highlight_text() : theme_text());
    }
  }
#else
  {
    int left_text = UI_CELL_MARGIN + TASK_CHECKBOX_SIZE + UI_CELL_MARGIN;
    int avail_w = bounds.size.w - left_text - UI_CELL_MARGIN;
    GSize tsz = graphics_text_layout_get_content_size_with_attributes(
        title, title_font, GRect(0, 0, avail_w, 500), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft, NULL);
    int title_h = tsz.h;
    if (title_h < 1) {
      title_h = 1;
    }
    int due_h = 0;
    if (due && due[0]) {
      GSize dsz = graphics_text_layout_get_content_size_with_attributes(
          due, due_font, GRect(0, 0, avail_w, 500), GTextOverflowModeTrailingEllipsis,
          GTextAlignmentLeft, NULL);
      due_h = dsz.h;
      if (due_h < 1) {
        due_h = 1;
      }
    }
    int total = title_h + (due && due[0] ? UI_TASK_DUE_GAP + due_h : 0);
    int y0 = list_row_y_nudged((bounds.size.h - total) / 2);
    GRect title_bounds = GRect(left_text, y0, avail_w, title_h);
    graphics_draw_text(ctx, title, title_font, title_bounds, GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
    if (due && due[0]) {
      graphics_context_set_text_color(ctx, hi ? theme_highlight_text() : theme_menu_subtle_text());
      GRect due_bounds =
          GRect(left_text, y0 + title_h + UI_TASK_DUE_GAP, avail_w, due_h);
      graphics_draw_text(ctx, due, due_font, due_bounds, GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentLeft, NULL);
      graphics_context_set_text_color(ctx, hi ? theme_highlight_text() : theme_text());
    }
  }
#endif

  ui_draw_tasks_checkbox_frame(ctx, cell_layer);
}
