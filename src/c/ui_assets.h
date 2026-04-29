#pragma once

#include <pebble.h>

void ui_assets_init(void);
void ui_assets_deinit(void);

const GBitmap *ui_assets_add_list(void);
const GBitmap *ui_assets_add_task(void);
const GBitmap *ui_assets_completed(void);
const GBitmap *ui_assets_trash(void);
