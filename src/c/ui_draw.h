#pragma once

#include <pebble.h>

int ui_draw_text_cell_width(const Layer *window_layer);
int ui_draw_task_row_text_layout_width(const Layer *window_layer);

void ui_draw_menu_title_row(GContext *ctx, const Layer *cell_layer, const char *title, bool accent_subtle,
                            GTextAttributes *round_flow_attr);
void ui_draw_menu_leading_icon_row(GContext *ctx, const Layer *cell_layer, const GBitmap *icon, const char *text,
                                    bool accent_subtle, GTextAttributes *round_flow_attr);
void ui_draw_add_labeled_row(GContext *ctx, const Layer *cell_layer, bool is_task_list,
                            GTextAttributes *round_attr);
void ui_draw_tasks_checkbox_frame(GContext *ctx, const Layer *cell_layer);
void ui_draw_tasks_open_cell(GContext *ctx, const Layer *cell_layer, const char *title,
                             GTextAttributes *round_flow);
